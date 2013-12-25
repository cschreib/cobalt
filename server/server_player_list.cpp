#include "server_player_list.hpp"
#include "server_player.hpp"
#include <time.hpp>

namespace server {
    player_list::player_list(netcom& net) : net_(net), max_player_(1u), pool_(net) {
        pool_ << net_.watch_request([&](netcom::request_t<request::client::join_players>& req) {
            if (players_.size() < max_player_) {
                actor_id_t id = req.from();
                std::string ip = net_.get_actor_ip(id);
                players_.emplace_back(id, ip, req.args.name, req.args.color, req.args.is_ai);
                player& p = players_.back();
                p.when_connected = now();

                req.answer();
                net_.send_message(server::netcom::all_actor_id,
                    make_packet<message::server::player_connected>(
                        id, ip, req.args.name, req.args.color, req.args.is_ai
                    )
                );
            } else {
                req.fail(request::client::join_players::failure{
                    request::client::join_players::failure::reason::too_many_players
                });
            }
        });

        pool_ << net_.watch_request([&](netcom::request_t<request::client::list_players>& req) {
            request::client::list_players::answer a;
            for (auto& p : players_) {
                a.players.push_back({p.id, p.ip, p.name, p.color, p.is_ai});
            }
            req.answer(std::move(a));
        });

        pool_ << net_.watch_message([&](message::server::internal::client_disconnected msg) {
            auto iter = players_.find_if([&](const player& p) { return p.id == msg.id; });
            if (iter == players_.end()) return;

            remove_player_(iter, message::server::player_disconnected::reason::connection_lost);
        });
    }

    void player_list::set_max_player(std::uint32_t max) {
        max_player_ = max;
    }

    void player_list::set_max_player(std::uint32_t max, auto_kick_policy p) {
        max_player_ = max;

        if (players_.size() <= max_player_) {
            return;
        }

        if (p.ai_first) {
            std::sort(players_.begin(), players_.end(), [](const player& p1, const player& p2) {
                return p1.is_ai < p2.is_ai;
            });
            auto first_ai = std::find_if(players_.begin(), players_.end(), [](const player& p) {
                return p.is_ai;
            });

            if (p.older_first) {
                std::sort(players_.begin(), first_ai, [](const player& p1, const player& p2) {
                    return p1.when_connected > p2.when_connected;
                });
                std::sort(first_ai, players_.end(), [](const player& p1, const player& p2) {
                    return p1.when_connected > p2.when_connected;
                });
            } else {
                std::sort(players_.begin(), first_ai, [](const player& p1, const player& p2) {
                    return p1.when_connected < p2.when_connected;
                });
                std::sort(first_ai, players_.end(), [](const player& p1, const player& p2) {
                    return p1.when_connected < p2.when_connected;
                });
            }
        } else {
            if (p.older_first) {
                std::sort(players_.begin(), players_.end(), [](const player& p1, const player& p2) {
                    return p1.when_connected > p2.when_connected;
                });
            } else {
                std::sort(players_.begin(), players_.end(), [](const player& p1, const player& p2) {
                    return p1.when_connected < p2.when_connected;
                });
            }
        }

        std::size_t rem_count = players_.size() - max_player_;
        while (rem_count != 0u) {
            remove_player_(players_.end()-1,
                message::server::player_disconnected::reason::auto_kicked
            );
            --rem_count;
        }
    }

    std::uint32_t player_list::max_player() const {
        return max_player_;
    }

    void player_list::remove_player_(ptr_vector<player>::iterator iter,
        message::server::player_disconnected::reason rsn) {

        actor_id_t id = iter->id;
        players_.erase(iter);
        net_.send_message(server::netcom::all_actor_id,
            make_packet<message::server::player_disconnected>(id, rsn)
        );
    }
}
