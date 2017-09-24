#include "client_netcom.hpp"
#include "server_netcom.hpp"
#include <config.hpp>

namespace client {
    netcom::netcom(config::state& conf, logger& out) :
        netcom_base(out), self_id_(invalid_actor_id), running_(false), connected_(false),
        terminate_thread_(false), sc_factory_(*this) {

        pool_ << conf.bind("netcom.debug_packets", debug_packets);
    }

    netcom::~netcom() {
        wait_for_shutdown();
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
        if (is_running()) {
            throw netcom_exception::already_running{};
        }

        address_ = addr;
        port_ = port;
        running_ = true;
        listener_thread_ = std::thread(&netcom::loop_, this);
    }

    void netcom::shutdown() {
        terminate_();
    }

    void netcom::wait_for_shutdown() {
        shutdown();

        do {
            process_packets(); // will join() if shutdown() didn't
            sf::sleep(sf::milliseconds(10));
        } while (running_);
    }

    void netcom::do_terminate_() {
        if (listener_thread_.joinable()) {
            terminate_thread_ = true;
            listener_thread_.join();
        }

        netcom_base::do_terminate_();
        sc_factory_.clear();
    }

    void netcom::loop_() {
        auto scfc = ctl::make_scoped([this]() {
            // Clean-up
            self_id_ = invalid_actor_id;
            terminate_thread_ = false;
            running_ = false;
        });

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
                    message::server::connection_failed::reason::unreachable
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

        auto sctc = ctl::scoped_toggle(connected_);

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
            while (output_.try_pop(op)) {
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
