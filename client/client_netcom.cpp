#include "client_netcom.hpp"
#include "server_netcom.hpp"
#include <iostream>

namespace client {
    netcom::netcom() : is_connected_(false), terminate_thread_(false),
        listener_thread_(std::bind(&netcom::loop_, this)) {}

    netcom::~netcom() {
        terminate();
    }

    bool netcom::is_connected() const {
        return is_connected_;
    }

    void netcom::run(const std::string& addr, std::uint16_t port) {
        terminate();
        terminate_thread_ = false;
        address_ = addr;
        port_ = port;
        listener_thread_.launch();
    }

    void netcom::terminate() {
        terminate_thread_ = true;
        listener_thread_.wait();
    }

    void netcom::loop_() {
        sf::TcpSocket socket;

        // Try to connect
        std::size_t wait_count = 5;
        while (wait_count != 0) {
            switch (socket.connect(sf::IpAddress(address_), port_, sf::seconds(1))) {
            case sf::Socket::Done : {
                send_message<message::server::connection_established>(self_actor_id);

                out_packet_t op(server_actor_id);
                if (socket.receive(op.impl) == sf::Socket::Done) {
                    out_packet_t top = op;
                    netcom_impl::packet_type t;
                    op >> t;
                    if (t != netcom_impl::packet_type::message) {
                        send_message<message::server::connection_denied>(self_actor_id,
                            message::server::connection_denied::reason::unexpected_packet
                        );
                        return;
                    }

                    message_packet_id_t id;
                    op >> id;
                    switch (id) {
                    case message::server::connection_granted::id :
                        input_.push(std::move(top.to_input()));
                        break;
                    case message::server::connection_denied::id :
                        input_.push(std::move(top.to_input()));
                        return;
                    default : 
                        send_message<message::server::connection_denied>(self_actor_id,
                            message::server::connection_denied::reason::unexpected_packet
                        );
                        return;
                    }
                } else {
                    send_message<message::server::connection_failed>(self_actor_id,
                        message::server::connection_failed::reason::cannot_authenticate
                    );
                    return;
                }

                wait_count = 0;
                break;
            }
            case sf::Socket::NotReady : break;
            case sf::Socket::Error : break;
            case sf::Socket::Disconnected :
                send_message<message::server::connection_failed>(self_actor_id,
                    message::server::connection_failed::reason::disconnected
                );
                return;
            }

            if (wait_count == 0) break;

            --wait_count;

            if (wait_count == 0) {
                send_message<message::server::connection_failed>(self_actor_id,
                    message::server::connection_failed::reason::timed_out
                );
                return;
            }
        }

        is_connected_ = true;

        // Enter main loop
        while (!terminate_thread_) {
            // Receive incoming packets
            in_packet_t ip(server_actor_id);
            socket.setBlocking(false);
            switch (socket.receive(ip.impl)) {
            case sf::Socket::Done :
                input_.push(std::move(ip));
                break;
            case sf::Socket::NotReady : break;
            case sf::Socket::Disconnected :
            case sf::Socket::Error :
                send_message<message::server::connection_failed>(self_actor_id,
                    message::server::connection_failed::reason::disconnected
                );
                terminate_thread_ = true;
                break;
            }

            if (terminate_thread_) break;

            // Send outgoing packets
            socket.setBlocking(true);
            out_packet_t op;
            while (output_.pop(op)) {
                switch (socket.send(op.impl)) {
                case sf::Socket::Done : break;
                case sf::Socket::NotReady :
                case sf::Socket::Disconnected :
                case sf::Socket::Error :
                    send_message<message::server::connection_failed>(self_actor_id,
                        message::server::connection_failed::reason::disconnected
                    );
                    terminate_thread_ = true;
                    break;
                }
            }

            sf::sleep(sf::milliseconds(5));
        }

        input_.clear();
        output_.clear();
        is_connected_ = false;
    }
}
