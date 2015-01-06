#include "server_netcom.hpp"
#include <config.hpp>
#include <time.hpp>
#include <string.hpp>

namespace server {
    netcom::connected_client_t::connected_client_t(std::unique_ptr<sf::TcpSocket> s, actor_id_t i) :
        socket(std::move(s)), id(i) {}

    netcom::client_t::client_t(actor_id_t i, std::string a) : id(i), ip(a) {}

    netcom::netcom(config::state& conf, logger& out) :
        netcom_base(out),
        conf_(conf), running_(false), connected_(false), connection_time_out_(5.0),
        listen_port_(4444), max_client_(1),
        client_id_provider_(max_client_, first_actor_id),
        shutdown_(false), shutdown_time_out_(3.0),
        listener_thread_(std::bind(&netcom::loop_, this)),
        sc_factory_(*this) {

        pool_ << conf_.bind("netcom.listen_port", listen_port_)
              << conf_.bind("netcom.connection.time_out", connection_time_out_)
              << conf_.bind("netcom.debug_packets", debug_packets)
              << conf_.bind("netcom.shutdown.time_out", shutdown_time_out_);

        pool_ << conf_.bind("netcom.max_client", [this](std::size_t max) {
            set_max_client_(max);
        }, max_client_);

        watch_message([this](const message::client_connected& msg) {
            clients_.insert(client_t{msg.id, msg.ip});
        });

        watch_message([this](const message::client_disconnected& msg) {
            clients_.erase(msg.id);
        });

        std::string credential_link_file = "cred_links.conf";
        conf.get_value("credential.links", credential_link_file, credential_link_file);
        read_credential_links_(credential_link_file);
    }

    netcom::~netcom() {
        wait_shutdown();
    }

    void netcom::set_max_client_(std::size_t max_client) {
        if (max_client_ == max_client) return;

        client_id_provider_.set_max_id(max_client);

        max_client_ = max_client;
    }

    void netcom::set_max_client(std::size_t max_client) {
        conf_.set_value("netcom.max_client", max_client);
    }

    bool netcom::is_running() const {
        return running_;
    }

    bool netcom::is_connected() const {
        return connected_;
    }

    void netcom::run() {
        run(listen_port_);
    }

    void netcom::run(std::uint16_t port) {
        if (running_) {
            throw netcom_exception::already_running{};
        }

        if (port != listen_port_) {
            conf_.set_value("netcom.listen_port", port);
        }

        running_ = true;
        listener_thread_.launch();
    }

    void netcom::shutdown() {
        if (running_) {
            watch_message([this](const message::server::internal::begin_terminate&) {
                send_message(self_actor_id,
                    make_packet<message::server::internal::do_terminate>()
                );
            });

            watch_message([this](const message::server::internal::do_terminate&) {
                terminate_();
            });

            shutdown_ = true;
            if (connected_) {
                send_message(all_actor_id,
                    make_packet<message::server::will_shutdown>(shutdown_time_out_)
                );
            }
        }
    }

    void netcom::wait_shutdown() {
        if (running_) {
            shutdown();
            while (running_) {
                process_packets();
                sf::sleep(sf::milliseconds(10));
            }
        }
    }

    void netcom::do_terminate_() {
        netcom_base::do_terminate_();
        client_id_provider_.clear();
        sc_factory_.clear();
        shutdown_countdown_ = 0.0;
        running_ = false;
    }

    std::string netcom::get_actor_ip(actor_id_t cid) const {
        if (cid == self_actor_id) {
            return "127.0.0.1";
        } else if (cid == all_actor_id) {
            return "broadcast";
        }

        auto iter = clients_.find(cid);
        if (iter == clients_.end()) {
            throw netcom_exception::invalid_actor{};
        }

        return iter->ip;
    }

    void netcom::grant_credentials(actor_id_t cid, const credential_list_t& creds) {
        if (cid == self_actor_id) {
            throw netcom_exception::invalid_actor{};
        }

        if (cid == all_actor_id) {
            for (auto& c : clients_) {
                c.cred.grant(creds);
                send_message(c.id, make_packet<message::credentials_granted>(creds));
            }
        } else {
            auto iter = clients_.find(cid);
            if (iter == clients_.end()) {
                throw netcom_exception::invalid_actor{};
            }

            iter->cred.grant(creds);
            send_message(iter->id, make_packet<message::credentials_granted>(creds));
        }
    }

    void netcom::remove_credentials(actor_id_t cid, const credential_list_t& creds) {
        if (cid == self_actor_id) {
            throw netcom_exception::invalid_actor{};
        }

        if (cid == all_actor_id) {
            for (auto& c : clients_) {
                c.cred.remove(creds);
                send_message(c.id, make_packet<message::credentials_removed>(creds));
            }
        } else {
            auto iter = clients_.find(cid);
            if (iter == clients_.end()) {
                throw netcom_exception::invalid_actor{};
            }

            iter->cred.remove(creds);
            send_message(iter->id, make_packet<message::credentials_removed>(creds));
        }
    }

    void netcom::loop_() {
        // Try to open the port
        while (listener_.listen(listen_port_) != sf::Socket::Done && !shutdown_) {
            send_message(self_actor_id,
                make_packet<message::server::internal::cannot_listen_port>(listen_port_)
            );

            sf::sleep(sf::seconds(connection_time_out_));
            if (shutdown_) return;
        }

        auto sc = ctl::scoped_toggle(connected_);

        send_message(self_actor_id,
            make_packet<message::server::internal::start_listening_port>(listen_port_)
        );

        selector_.add(listener_);

        // Start the main loop
        bool stop = false;
        double last = 0.0;

        while (!stop) {
            selector_.wait(sf::milliseconds(10));

            // Look for new clients
            if (!shutdown_ && selector_.isReady(listener_)) {
                std::unique_ptr<sf::TcpSocket> s(new sf::TcpSocket());
                if (listener_.accept(*s) == sf::Socket::Done) {
                    if (connected_clients_.size() < max_client_) {
                        actor_id_t id;
                        if (client_id_provider_.make_id(id)) {
                            send_message(self_actor_id,
                                make_packet<message::client_connected>(
                                    id, s->getRemoteAddress().toString()
                                )
                            );

                            out_packet_t p = create_message(
                                make_packet<message::server::connection_granted>(id)
                            );
                            s->send(p.impl);

                            selector_.add(*s);
                            connected_clients_.insert(connected_client_t(std::move(s), id));
                        } else {
                            out_packet_t p = create_message(
                                make_packet<message::server::connection_denied>(
                                    message::server::connection_denied::reason::too_many_clients
                                )
                            );
                            s->send(p.impl);
                        }
                    } else {
                        out_packet_t p = create_message(
                            make_packet<message::server::connection_denied>(
                                message::server::connection_denied::reason::too_many_clients
                            )
                        );
                        s->send(p.impl);
                    }
                }
            }

            std::vector<actor_id_t> remove_list;

            // Receive packets from each clients
            for (auto& c : connected_clients_) {
                auto& s = c.socket;
                if (selector_.isReady(*s)) {
                    sf::Packet p;
                    switch (s->receive(p)) {
                    case sf::Socket::Done : {
                        in_packet_t ip(c.id);
                        ip.impl = std::move(p);
                        input_.push(std::move(ip));
                        break;
                    }
                    case sf::Socket::Disconnected :
                    case sf::Socket::Error :
                        remove_list.push_back(c.id);
                        break;
                    case sf::Socket::NotReady :
                    default : break;
                    }
                }
            }

            // Send packets to clients
            out_packet_t op;
            while (output_.pop(op)) {
                if (op.to == all_actor_id) {
                    // Send to all clients
                    for (auto& c : connected_clients_) {
                        auto& s = c.socket;
                        switch (s->send(op.impl)) {
                        case sf::Socket::Done : break;
                        case sf::Socket::Disconnected :
                        case sf::Socket::Error :
                            remove_list.push_back(c.id);
                            break;
                    case sf::Socket::NotReady :
                        default : break;
                        }
                    }
                    // Including oneself
                    input_.push(std::move(op.to_input()));
                } else if (op.to == self_actor_id) {
                    // Bounce back packets sent to oneself
                    input_.push(std::move(op.to_input()));
                } else {
                    // Send to individual clients
                    auto iter = connected_clients_.find(op.to);
                    if (iter == connected_clients_.end()) {
                        out_packet_t tp = create_message(
                            make_packet<message::server::internal::unknown_client>(
                                op.to
                            )
                        );
                        input_.push(std::move(tp.to_input()));
                        continue;
                    };

                    auto& s = iter->socket;
                    switch (s->send(op.impl)) {
                    case sf::Socket::Done : break;
                    case sf::Socket::Disconnected :
                    case sf::Socket::Error :
                        remove_list.push_back(iter->id);
                        break;
                    case sf::Socket::NotReady :
                    default : break;
                    }
                }
            }

            // Remove disconnected clients
            for (auto cid : remove_list) {
                remove_client_(cid);
            }

            if (shutdown_) {
                if (connected_clients_.empty()) {
                    stop = true;
                } else {
                    if (last == 0.0) {
                        last = now();
                    }

                    double thetime = now();
                    double dt = thetime - last;
                    last = thetime;

                    shutdown_countdown_ += dt;
                    if (shutdown_countdown_ >= shutdown_time_out_) {
                        stop = true;
                    }
                }
            }
        }

        selector_.clear();
        listener_.close();

        out_packet_t msg = create_message(
            make_packet<message::server::internal::do_terminate>());
        input_.push(std::move(msg.to_input()));
    }

    void netcom::remove_client_(actor_id_t cid) {
        auto iter = connected_clients_.find(cid);
        if (iter == connected_clients_.end()) return;
        remove_client_(iter);
    }

    void netcom::remove_client_(connected_client_list_t::iterator ic) {
        send_message(self_actor_id,
            make_packet<message::client_disconnected>(
                ic->id, message::client_disconnected::reason::connection_lost
            )
        );

        client_id_provider_.free_id(ic->id);
        selector_.remove(*ic->socket);
        ic->socket->disconnect();
        connected_clients_.erase(ic);
    }

    void netcom::read_credential_links_(const std::string& file_name) {
        std::ifstream file(file_name);

        std::string line;
        std::size_t l = 0;
        while (std::getline(file, line)) {
            ++l;

            line = string::trim(line);
            if (line.empty() || line[0] == '#') continue;

            auto spl = string::split(line, "->");
            if (spl.size() != 2) {
                out_.warning("ill-formed line is ignored in '", file_name, ":", l, "'");
                out_.note(line);
                continue;
            }

            credential_t c1 = string::trim(spl[0]);
            auto iter = credential_links_.find(c1);
            if (iter == credential_links_.end()) {
                iter = credential_links_.insert({c1, {}});
            }

            credential_t c2 = string::trim(spl[1]);
            iter->links.insert(c2);
        }
    }

    bool netcom::credential_implies_(const credential_t& c1, const credential_t& c2) const {
        if (c1 == c2) {
            return true;
        }

        auto iter = credential_links_.find(c1);
        if (iter == credential_links_.end()) {
            return false;
        }

        return iter->links.find(c2) != iter->links.end();
    }

    credential_list_t netcom::get_missing_credentials_(actor_id_t cid,
        const constant_credential_list_t& lst) const {

        const client_t& client = [&]() {
            auto iter = clients_.find(cid);
            if (iter == clients_.end()) {
                throw netcom_exception::invalid_actor{};
            }

            return *iter;
        }();

        credential_list_t missing;

        for (auto cc : lst) {
            credential_t c = cc;

            auto iter = std::find_if(client.cred.begin(), client.cred.end(),
                [this,c](const credential_t& ic) {
                return credential_implies_(ic, c);
            });

            if (iter == client.cred.end()) {
                missing.grant(c);
            }
        }

        return missing;
    }
}
