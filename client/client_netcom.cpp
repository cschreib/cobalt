#include "client_netcom.hpp"
#include "server_netcom.hpp"

namespace client {
    netcom::netcom() : self_id_(invalid_actor_id), running_(false), connected_(false),
        terminate_thread_(false),
        listener_thread_(std::bind(&netcom::loop_, this)), sc_factory_(*this) {}

    netcom::~netcom() {
        shutdown();
    }

    actor_id_t netcom::self_id() const {
        return self_id_;
    }

    bool netcom::is_running() const {
        return running_;
    }

    bool netcom::is_connected() const {
        return connected_;
    }

    void netcom::run(const std::string& addr, std::uint16_t port) {
        if (running_) {
            throw netcom_exception::already_running{};
        }

        watch_message([this](const message::server::will_shutdown&) {
            shutdown();
        });

        terminate_thread_ = false;
        address_ = addr;
        port_ = port;
        running_ = true;
        listener_thread_.launch();
    }

    void netcom::shutdown() {
        if (running_) {
            terminate_();
        }
    }

    void netcom::do_terminate_() {
        terminate_thread_ = true;
        listener_thread_.wait();

        netcom_base::do_terminate_();
        sc_factory_.clear();
        self_id_ = invalid_actor_id;
        running_ = false;
    }

    void netcom::loop_() {
        sf::TcpSocket socket;

        // Try to connect
        std::size_t wait_count = 5;
        while (wait_count != 0) {
            switch (socket.connect(sf::IpAddress(address_), port_, sf::seconds(1))) {
            case sf::Socket::Done : {
                send_message(self_actor_id, message::server::connection_established{});

                out_packet_t op(server_actor_id);
                if (socket.receive(op.impl) == sf::Socket::Done) {
                    auto opv = op.view();
                    netcom_impl::packet_type t;
                    opv >> t;
                    if (t != netcom_impl::packet_type::message) {
                        send_message(self_actor_id, make_packet<message::server::connection_denied>(
                            message::server::connection_denied::reason::unexpected_packet
                        ));
                        return;
                    }

                    packet_id_t id;
                    opv >> id;
                    switch (id) {
                    case message::server::connection_granted::packet_id__ :
                        opv >> self_id_;
                        input_.push(std::move(op.to_input()));
                        break;
                    case message::server::connection_denied::packet_id__ :
                        input_.push(std::move(op.to_input()));
                        return;
                    default :
                        send_message(self_actor_id, make_packet<message::server::connection_denied>(
                            message::server::connection_denied::reason::unexpected_packet
                        ));
                        return;
                    }
                } else {
                    send_message(self_actor_id, make_packet<message::server::connection_failed>(
                        message::server::connection_failed::reason::cannot_authenticate
                    ));
                    return;
                }

                wait_count = 0;
                break;
            }
            case sf::Socket::NotReady : break;
            case sf::Socket::Error : break;
            case sf::Socket::Disconnected :
                send_message(self_actor_id, make_packet<message::server::connection_failed>(
                    message::server::connection_failed::reason::disconnected
                ));
                return;
            }

            if (wait_count == 0) break;

            --wait_count;

            if (wait_count == 0) {
                send_message(self_actor_id, make_packet<message::server::connection_failed>(
                    message::server::connection_failed::reason::timed_out
                ));
                return;
            }
        }

        auto sc = ctl::scoped_toggle(connected_);

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
                send_message(self_actor_id, make_packet<message::server::connection_failed>(
                    message::server::connection_failed::reason::disconnected
                ));
                terminate_thread_ = true;
                break;
            }

            if (terminate_thread_) break;

            // Send outgoing packets
            socket.setBlocking(true);
            out_packet_t op;
            while (output_.pop(op)) {
                if (op.to == server_actor_id) {
                    switch (socket.send(op.impl)) {
                    case sf::Socket::Done : break;
                    case sf::Socket::NotReady :
                    case sf::Socket::Disconnected :
                    case sf::Socket::Error :
                        send_message(self_actor_id, make_packet<message::server::connection_failed>(
                            message::server::connection_failed::reason::disconnected
                        ));
                        terminate_thread_ = true;
                        break;
                    }
                } else if (op.to == self_actor_id) {
                    // Bounce back packets sent to oneself
                    input_.push(std::move(op.to_input()));
                } else if (op.to == all_actor_id) {
                    // Clients to not have the right to broadcast
                    throw netcom_exception::invalid_actor();
                } else {
                    // Peer to peer communication is not supported for now
                    throw netcom_exception::invalid_actor();
                }
            }

            sf::sleep(sf::milliseconds(5));
        }
    }
}
