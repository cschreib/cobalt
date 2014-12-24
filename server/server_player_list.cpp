#include "server_player_list.hpp"
#include "server_player.hpp"
#include <time.hpp>
#include <config.hpp>

namespace server {
    player_list::player_list(netcom& net, config::state& conf) :
        net_(net), conf_(conf), max_player_(1u),
        collection_(net.make_shared_collection<player_collection_traits>("server_player_list")) {

        collection_.make_collection_packet([this](packet::player_list& lst) {
            for (auto& p : players_) {
                lst.players.push_back({p.id, p.ip, p.name, p.color, p.is_ai});
            }
        });

        pool_ << net_.watch_request(
            [this](netcom::request_t<request::client::player_list_collection_id>&& req) {
                req.answer(collection_.id());
            }
        );

        collection_.connect();

        pool_ << conf_.bind("player_list.max_player", max_player_);

        pool_ << net_.watch_request(
            [this](netcom::request_t<request::client::join_players>&& req) {
                if (players_.size() < max_player_) {
                    actor_id_t id = req.packet.from;
                    std::string ip = net_.get_actor_ip(id);
                    players_.emplace_back(id, ip, req.arg.name, req.arg.color, req.arg.is_ai);
                    player& p = players_.back();
                    p.when_connected = now();

                    req.answer();

                    collection_.add_item(id, ip, req.arg.name, req.arg.color, req.arg.is_ai);
                } else {
                    req.fail(request::client::join_players::failure::reason::too_many_players);
                }
            }
        );

        pool_ << net_.watch_request(
            [this](netcom::request_t<request::client::leave_players>&& req) {
                for (auto iter = players_.begin(); iter != players_.end(); ++iter) {
                    if (iter->id == req.packet.from) {
                        remove_player_(iter, packet::player_disconnected::reason::left);
                        req.answer();
                        return;
                    }
                }
                req.fail();
            }
        );

        pool_ << net_.watch_message(
            [this](const message::client_disconnected& msg) {
                auto iter = players_.find_if([&](const player& p) { return p.id == msg.id; });
                if (iter == players_.end()) return;

                remove_player_(iter, packet::player_disconnected::reason::connection_lost);
            }
        );
    }

    void player_list::set_max_player(std::uint32_t max) {
        conf_.set_value("player_list.max_player", max);
    }

    void player_list::set_max_player(std::uint32_t max, auto_kick_policy p) {
        conf_.set_value("player_list.max_player", max);

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
            remove_player_(players_.end()-1, packet::player_disconnected::reason::auto_kicked);
            --rem_count;
        }
    }

    std::uint32_t player_list::max_player() const {
        return max_player_;
    }

    void player_list::remove_player_(ctl::ptr_vector<player>::iterator iter,
        packet::player_disconnected::reason rsn) {

        actor_id_t id = iter->id;
        players_.erase(iter);
        collection_.remove_item(id, rsn);
    }
}
