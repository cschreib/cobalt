#include "client_player_list.hpp"
#include <server_player_list.hpp>
#include "client_player.hpp"

namespace client {
    player_list::player_list(netcom& net) : net_(net), self_(nullptr) {
        collection_.on_received =
        [&](const netcom::request_answer_t<request::client::list_players>& msg) {
            if (msg.failed) return;
            for (auto& p : msg.answer.players) {
                players_.emplace_back(p.id, p.ip, p.name, p.color, p.is_ai);
            }
        };

        collection_.on_add_item = [this](const message::server::player_connected& msg) {
            players_.emplace_back(msg.id, msg.ip, msg.name, msg.color, msg.is_ai);
            if (msg.id == net_.self_id()) {
                self_ = &players_.back();
                on_join.dispatch(*self_);
            }

            on_player_connected.dispatch(players_.back());
        };

        collection_.on_remove_item = [this](const message::server::player_disconnected& msg) {
            auto iter = players_.find_if([&](const player& p) { return p.id == msg.id; });
            on_player_disconnected.dispatch(*iter);
            players_.erase(iter);
        };

        collection_.connect(net);
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
        auto make_request = [this,name,col,as_ai]() {
            pool_ << net_.send_request(client::netcom::server_actor_id,
                make_packet<request::client::join_players>(name, col, as_ai),
                [this](const netcom::request_answer_t<request::client::join_players>& msg) {
                    if (msg.failed) {
                        on_join_fail.dispatch();
                    }
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

    void player_list::leave() {
        pool_ << net_.send_request(client::netcom::server_actor_id,
            make_packet<request::client::leave_players>(),
            [this](const netcom::request_answer_t<request::client::leave_players>& msg) {
                if (!msg.failed) {
                    self_ = nullptr;
                    on_leave.dispatch();
                }
            }
        );
    }

    bool player_list::is_joined() const {
        return self_ != nullptr;
    }

    const player& player_list::get_self() const {
        return *self_;
    }
}
