#ifndef CLIENT_SHARED_COLLECTION_HPP
#define CLIENT_SHARED_COLLECTION_HPP

#include <connection_handler.hpp>
#include "client_netcom.hpp"

namespace client {
    template<typename ElementTraits>
    class shared_collection {
        using full_collection_packet_request = typename ElementTraits::full_packet;
        using full_collection_packet_answer = netcom::request_answer_t<
            full_collection_packet_request>;
        using add_collection_element_packet = typename ElementTraits::add_packet;
        using remove_collection_element_packet = typename ElementTraits::remove_packet;
        using quit_collection_packet = typename ElementTraits::quit_packet;

        netcom* net_ = nullptr;
        scoped_connection_pool collection_pool_;

    public :
        shared_collection() :
            on_received([](const full_collection_packet_answer&){}),
            on_add_item([](const add_collection_element_packet&){}),
            on_remove_item([](const remove_collection_element_packet&){}) {}

        ~shared_collection() noexcept {
            disconnect();
        }

        ctl::delegate<void(const full_collection_packet_answer&)>    on_received;
        ctl::delegate<void(const add_collection_element_packet&)>    on_add_item;
        ctl::delegate<void(const remove_collection_element_packet&)> on_remove_item;

        template<typename ... Args>
        void connect(netcom& net, Args&& ... args) {
            disconnect();

            net_ = &net;

            collection_pool_ << net_->send_request(netcom_base::server_actor_id,
                make_packet<full_collection_packet_request>(std::forward<Args>(args)...),
                on_received
            );

            collection_pool_ << net_->watch_message(on_add_item);
            collection_pool_ << net_->watch_message(on_remove_item);
        }

        void disconnect() {
            if (!net_) return;
            net_->send_message(netcom_base::server_actor_id, make_packet<quit_collection_packet>());
            collection_pool_.stop_all();
            net_ = nullptr;
        }
    };
}

#endif
