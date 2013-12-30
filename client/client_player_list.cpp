#include "client_player_list.hpp"
#include <server_player_list.hpp>
#include "client_player.hpp"

namespace client {
    player_list::player_list(netcom& net) : net_(net), self_(nullptr) {
        // pool_.hold_all();
        net_.send_request(netcom::server_actor_id,
            make_packet<request::client::list_players>(),
            [&](const request::client::list_players::answer& msg) {
                for (auto& p : msg.players) {
                    players_.emplace_back(p.id, p.ip, p.name, p.color, p.is_ai);
                }

                // pool_.release_all();
            }
        );

        pool_ << net_.watch_message([&](const message::server::player_connected& msg) {
            players_.emplace_back(msg.id, msg.ip, msg.name, msg.color, msg.is_ai);
            if (msg.id == net_.self_id()) {
                self_ = &players_.back();
            }
        });

        pool_ << net_.watch_message([&](const message::server::player_disconnected& msg) {
            auto iter = players_.find_if([&](const player& p) { return p.id == msg.id; });
            players_.erase(iter);
        });
    }

    bool player_list::is_player(actor_id_t id) const {
        auto iter = players_.find_if([&](const player& p) { return p.id == id; });
        return iter != players_.end();
    }

    const player& player_list::get_player(actor_id_t id) const {
        auto iter = players_.find_if([&](const player& p) { return p.id == id; });
        return *iter;
    }

    void player_list::join_as(const std::string& name, const color32& col, bool as_ai) {
        auto make_request = [=]() {
            pool_ << net_.send_request(client::netcom::server_actor_id,
                make_packet<request::client::join_players>(name, col, as_ai),
                [](const request::client::join_players::answer& msg) {

                },
                [](const request::client::join_players::failure& msg) {

                }, []() {

                }
            );
        };

        if (net_.is_connected()) {
            make_request();
        } else {
            pool_ << net_.watch_message<watch_policy::once>(
                [=](message::server::connection_granted) {
                make_request();
            });
        }
    }

    bool player_list::is_joined() const {
        return self_ != nullptr;
    }

    const player& player_list::get_self() const {
        return *self_;
    }
}
