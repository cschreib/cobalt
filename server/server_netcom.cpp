#include "server_netcom.hpp"
#include <config.hpp>

namespace server {
    netcom::client_t::client_t(std::unique_ptr<sf::TcpSocket> s, actor_id_t i) :
        socket(std::move(s)), id(i) {}

    netcom::netcom(config::state& conf) :
        conf_(conf), connected_(false), connection_time_out_(5.0),
        listen_port_(4444), max_client_(1),
        client_id_provider_(max_client_, first_actor_id),
        terminate_thread_(false), listener_thread_(std::bind(&netcom::loop_, this)),
        sc_factory_(*this) {

        pool_ << conf_.bind("netcom.listen_port", listen_port_);
        pool_ << conf_.bind("netcom.connection.time_out", connection_time_out_);
        pool_ << conf_.bind("netcom.max_client", [this](std::size_t max) {
            set_max_client_(max);
        }, max_client_);
    }

    netcom::~netcom() {
        terminate();
    }

    void netcom::set_max_client_(std::size_t max_client) {
        if (max_client_ == max_client) return;

        client_id_provider_.set_max_id(max_client);

        max_client_ = max_client;
    }

    void netcom::set_max_client(std::size_t max_client) {
        conf_.set_value("netcom.max_client", max_client);
    }

    bool netcom::is_connected() const {
        return connected_;
    }

    void netcom::run() {
        run(listen_port_);
    }

    void netcom::run(std::uint16_t port) {
        if (connected_) {
            terminate();
        }
        terminate_thread_ = false;

        if (port != listen_port_) {
            conf_.set_value("netcom.listen_port", port);
        }
        listener_thread_.launch();
    }

    void netcom::terminate_() {
        terminate_thread_ = true;
        listener_thread_.wait();
        client_id_provider_.clear();
        netcom_base::terminate_();
    }

    std::string netcom::get_actor_ip(actor_id_t cid) const {
        if (cid == self_actor_id) {
            return "127.0.0.1";
        } else if (cid == all_actor_id) {
            return "...";
        }

        auto iter = clients_.find(cid);
        if (iter == clients_.end() || !iter->socket) {
            return "?";
        }

        return iter->socket->getRemoteAddress().toString();
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
        while (listener_.listen(listen_port_) != sf::Socket::Done && !terminate_thread_) {
            send_message(self_actor_id,
                make_packet<message::server::internal::cannot_listen_port>(listen_port_)
            );

            sf::sleep(sf::seconds(connection_time_out_));
            if (terminate_thread_) return;
        }

        connected_ = true;

        send_message(self_actor_id,
            make_packet<message::server::internal::start_listening_port>(listen_port_)
        );

        selector_.add(listener_);

        // Start the main loop
        while (!terminate_thread_) {
            selector_.wait(sf::milliseconds(10));

            // Look for new clients
            if (selector_.isReady(listener_)) {
                std::unique_ptr<sf::TcpSocket> s(new sf::TcpSocket());
                if (listener_.accept(*s) == sf::Socket::Done) {
                    if (clients_.size() < max_client_) {
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
                            clients_.insert(client_t(std::move(s), id));
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
            for (auto& c : clients_) {
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
                    for (auto& c : clients_) {
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
                } else if (op.to == self_actor_id) {
                    // Bounce back packets sent to oneself
                    input_.push(std::move(op.to_input()));
                } else {
                    // Send to individual clients
                    auto iter = clients_.find(op.to);
                    if (iter == clients_.end()) {
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
                send_message(self_actor_id,
                    make_packet<message::client_disconnected>(
                        cid, message::client_disconnected::reason::connection_lost
                    )
                );

                remove_client_(cid);
            }
        }

        while (!clients_.empty()) {
            remove_client_(clients_.begin());
        }

        input_.clear();
        output_.clear();
        selector_.clear();
        listener_.close();

        connected_ = false;
    }

    void netcom::remove_client_(actor_id_t cid) {
        auto iter = clients_.find(cid);
        if (iter == clients_.end()) return;
        remove_client_(iter);
    }

    void netcom::remove_client_(client_list_t::iterator ic) {
        client_id_provider_.free_id(ic->id);
        selector_.remove(*ic->socket);
        ic->socket->disconnect();
        clients_.erase(ic);
    }

    credential_list_t netcom::get_missing_credentials_(actor_id_t cid,
        const constant_credential_list_t& lst) {

        auto iter = clients_.find(cid);
        if (iter == clients_.end()) {
            throw netcom_exception::invalid_actor{};
        }

        return iter->cred.find_missing(lst);
    }
}
