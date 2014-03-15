#ifndef SERVER_SHARED_COLLECTION_HPP
#define SERVER_SHARED_COLLECTION_HPP

#include <connection_handler.hpp>
#include <sorted_vector.hpp>
#include "server_netcom.hpp"

namespace server {
    /// Collection of objects shared over the network.
    template<typename ElementTraits>
    class shared_collection {
        using full_collection_packet_request = typename ElementTraits::full_packet;
        using full_collection_packet_answer = typename full_collection_packet_request::answer;
        using full_collection_packet_failure = typename full_collection_packet_request::failure;
        using add_collection_element_packet = typename ElementTraits::add_packet;
        using remove_collection_element_packet = typename ElementTraits::remove_packet;
        using quit_collection_packet = typename ElementTraits::quit_packet;
        using request_type = netcom::request_t<full_collection_packet_request>;
        using message_type = netcom::message_t<quit_collection_packet>;

        netcom* net_ = nullptr;
        scoped_connection_pool collection_pool_;
        ctl::sorted_vector<actor_id_t> clients_;

    public :
        shared_collection() :
            make_collection_packet([](request_type&& req){}) {}

        template<typename ... Args>
        void add_item(Args&& ... args) const {
            if (!net_) return;
            auto p = make_packet<add_collection_element_packet>(std::forward<Args>(args)...);
            for (auto id : clients_) {
                net_->send_message(id, p);
            }
        }

        template<typename ... Args>
        void remove_item(Args&& ... args) const {
            if (!net_) return;
            auto p = make_packet<remove_collection_element_packet>(std::forward<Args>(args)...);
            for (auto id : clients_) {
                net_->send_message(id, p);
            }
        }

        ctl::delegate<void(request_type&& req)> make_collection_packet;

        void connect(netcom& net) {
            disconnect();

            net_ = &net;

            collection_pool_ << net_->watch_request([this](request_type&& req) {
                clients_.insert(req.from);
                make_collection_packet(std::move(req));
            });

            collection_pool_ << net_->watch_message([this](const message_type& msg) {
                clients_.erase(msg.from);
            });

            collection_pool_ << net_->watch_message([this](
                const message::server::internal::client_disconnected& msg) {
                auto iter = clients_.find(msg.id);
                if (iter == clients_.end()) return;
                clients_.erase(iter);
            });
        }

        void disconnect() {
            clients_.clear();
            collection_pool_.stop_all();
            net_ = nullptr;
        }
    };
}

#endif
