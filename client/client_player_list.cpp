#include "client_player_list.hpp"
#include <server_player_list.hpp>
#include "client_player.hpp"

namespace client {
    player_list::player_list(netcom& net) : net_(net), self_(nullptr) {
        pool_ << net_.send_request(netcom::server_actor_id,
            make_packet<request::client::player_list_collection_id>(),
            [this](const request::client::player_list_collection_id::answer& ans) {
                collection_ = net_.make_shared_collection_observer<player_collection_traits>(ans.id);

                collection_.on_received.connect([this](const packet::player_list& lst) {
                    for (auto& p : lst.players) {
                        players_.emplace_back(p.id, p.ip, p.name, p.color, p.is_ai);
                    }
                    // on_list_received.dispatch();
                });

                collection_.on_add_item.connect([this](const packet::player_connected& p) {
                    players_.emplace_back(p.id, p.ip, p.name, p.color, p.is_ai);
                    if (p.id == net_.self_id()) {
                        self_ = &players_.back();
                        on_join.dispatch(*self_);
                    }

                    on_player_connected.dispatch(players_.back());
                });

                collection_.on_remove_item.connect([this](const packet::player_disconnected& p) {
                    auto iter = players_.find_if([&](const player& tp) { return tp.id == p.id; });
                    on_player_disconnected.dispatch(*iter);
                    players_.erase(iter);
                });

                // collection_.on_register_unhandled = []() { print("unhandled?!"); };
                // collection_.on_register_fail = [](ctl::empty_t) { print("failed!!"); };

                collection_.connect(netcom::server_actor_id);
            }
        );
    }

    bool player_list::is_player(actor_id_t id) const {
        auto iter = players_.find_if([&](const player& p) { return p.id == id; });
        return iter != players_.end();
    }

    const player& player_list::get_player(actor_id_t id) const {
        auto iter = players_.find_if([&](const player& p) { return p.id == id; });
        return *iter;
    }

    void player_list::request_join_(const std::string& name, const color32& col, bool as_ai) {
        pool_ << net_.send_request(netcom::server_actor_id,
            make_packet<request::client::join_players>(name, col, as_ai),
            [this](const netcom::request_answer_t<request::client::join_players>& msg) {
                if (msg.failed) {
                    on_join_fail.dispatch();
                }
            }
        );
    }

    void player_list::join_as(const std::string& name, const color32& col, bool as_ai) {
        if (collection_.is_connected()) {
            request_join_(name, col, as_ai);
        } else {
            pool_ << collection_.on_received.connect<unique_signal_connection>(
                [=](const packet::player_list&) {
                    request_join_(name, col, as_ai);
                }
            );
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
