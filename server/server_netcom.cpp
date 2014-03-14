#include "server_netcom.hpp"
#include <config.hpp>
#include <print.hpp>

namespace server {
    netcom::netcom(config::state& conf) :
        conf_(conf), listen_port_(4444), max_client_(1), is_connected_(false),
        terminate_thread_(false), listener_thread_(std::bind(&netcom::loop_, this)) {

        pool_ << conf_.bind("netcom.listen_port", listen_port_);
        pool_ << conf_.bind("netcom.max_client", [this](std::size_t max) {
            set_max_client_(max);
        }, max_client_);

        available_ids_.reserve(max_client_);
        for (std::size_t i = 0; i < max_client_; ++i) {
            available_ids_.insert(max_client_ - i - 1 + first_actor_id);
        }
    }

    netcom::~netcom() {
        terminate();
    }

    void netcom::set_max_client_(std::size_t max_client) {
        if (max_client_ == max_client) return;

        if (max_client_ < max_client) {
            // Allocate new ids
            available_ids_.reserve(max_client);
            for (std::size_t i = max_client_; i < max_client; ++i) {
                available_ids_.insert(max_client - i - 1 + first_actor_id);
            }
        } else {
            // Remove ids that are no longer accessible
            if (max_client == 0) {
                available_ids_.clear();
            } else {
                for (std::size_t i = max_client_ - 1; i >= max_client; --i) {
                    available_ids_.erase(max_client - i - 1 + first_actor_id);
                }
            }
        }

        max_client_ = max_client;
    }

    void netcom::set_max_client(std::size_t max_client) {
        conf_.set_value("netcom.max_client", max_client);
    }

    bool netcom::is_connected() const {
        return is_connected_;
    }

    void netcom::run() {
        run(listen_port_);
    }

    void netcom::run(std::uint16_t port) {
        if (is_connected_) {
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

    void netcom::loop_() {
        // Try to open the port
        while (listener_.listen(listen_port_) != sf::Socket::Done && !terminate_thread_) {
            send_message(self_actor_id,
                make_packet<message::server::internal::cannot_listen_port>(listen_port_)
            );

            sf::sleep(sf::seconds(5));
            if (terminate_thread_) return;
        }

        is_connected_ = true;

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
                        if (make_id_(id)) {
                            send_message(self_actor_id,
                                make_packet<message::server::internal::client_connected>(
                                    id, s->getRemoteAddress().toString()
                                )
                            );

                            out_packet_t p = create_message_(
                                make_packet<message::server::connection_granted>(id)
                            );
                            s->send(p.impl);

                            selector_.add(*s);
                            clients_.insert({std::move(s), id});
                        } else {
                            out_packet_t p = create_message_(
                                make_packet<message::server::connection_denied>(
                                    message::server::connection_denied::reason::too_many_clients
                                )
                            );
                            s->send(p.impl);
                        }
                    } else {
                        out_packet_t p = create_message_(
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
                } else {
                    // Send to individual clients
                    auto iter = clients_.find(op.to);
                    if (iter == clients_.end()) {
                        out_packet_t tp = create_message_(
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
                    make_packet<message::server::internal::client_disconnected>(
                        cid, message::server::internal::client_disconnected::reason::connection_lost
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

        is_connected_ = false;
    }

    bool netcom::make_id_(actor_id_t& id) {
        if (available_ids_.empty()) return false;
        id = available_ids_.back();
        available_ids_.pop_back();
        return true;
    }

    void netcom::free_id_(actor_id_t id) {
        if (id < max_client_) {
            available_ids_.insert(id);
        }
    }

    void netcom::remove_client_(actor_id_t cid) {
        auto iter = clients_.find(cid);
        if (iter == clients_.end()) return;
        remove_client_(iter);
    }

    void netcom::remove_client_(client_list_t::iterator ic) {
        free_id_(ic->id);
        selector_.remove(*ic->socket);
        ic->socket->disconnect();
        clients_.erase(ic);
    }
}
