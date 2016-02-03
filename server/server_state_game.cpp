#include "server_state_game.hpp"
#include "server_state_iddle.hpp"
#include "server_instance.hpp"
#include <filesystem.hpp>

namespace server {
namespace state {
    game::game(server::instance& serv) : base_impl(serv, "game") {
        // TODO: add all game components to this container
        save_chunks_.push_back(universe_.make_serializer());

        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::game_save>&& req) {
            try {
                save_to_directory(req.arg.save);
                req.answer();
            } catch (request::server::game_save::failure& fail) {
                // Clear load buffers
                for (auto& c : save_chunks_) {
                    c.clear();
                }

                req.fail(std::move(fail));
            } catch (...) {
                // Clear load buffers
                for (auto& c : save_chunks_) {
                    c.clear();
                }

                out_.error("unexpected exception in game::save_to_directory()");
                throw;
            }
        });

        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::game_load>&& req) {
            try {
                load_from_directory(req.arg.save);
                req.answer();
            } catch (request::server::game_load::failure& fail) {
                // Clear load buffers
                for (auto& c : save_chunks_) {
                    c.clear();
                }

                req.fail(std::move(fail));
            } catch (...) {
                // Clear load buffers
                for (auto& c : save_chunks_) {
                    c.clear();
                }

                out_.error("unexpected exception in game::load_from_directory()");
                throw;
            }
        });

        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::stop_and_iddle>&& req) {
            serv_.set_state<server::state::iddle>();
            req.answer();
        });
    }

    void game::save_to_directory(const std::string& dir) {
        using failure = request::server::game_save::failure;

        if (saving_) {
            throw failure{failure::reason::already_saving, ""};
        }

        saving_ = true;

        // Block the game and bake all game data into serializable structures
        net_.send_message(netcom::all_actor_id, make_packet<message::server::game_save_progress>(
            message::server::game_save_progress::step::gathering_game_data
        ));

        for (auto& c : save_chunks_) {
            c.save_data();
        }

        // Save to disk in the background
        net_.send_message(netcom::all_actor_id, make_packet<message::server::game_save_progress>(
            message::server::game_save_progress::step::saving_to_disk
        ));

        thread_ = std::thread([this, dir]() {
            for (auto& c : save_chunks_) {
                c.serialize(dir);
            }

            // Clear buffers
            for (auto& c : save_chunks_) {
                c.clear();
            }

            net_.send_message(netcom::all_actor_id, make_packet<message::server::game_save_progress>(
                message::server::game_save_progress::step::game_saved
            ));

            saving_ = false;
        });
    }

    void game::load_from_directory(const std::string& dir) {
        using failure = request::server::game_load::failure;

        // The whole game will be blocked during the operation.

        if (saving_) {
            throw failure{failure::reason::cannot_load_while_saving, ""};
        }

        if (!file::exists(dir)) {
            throw failure{failure::reason::no_such_saved_game, ""};
        }

        if (!is_saved_game_directory(dir)) {
            throw failure{failure::reason::invalid_saved_game, ""};
        }

        // NOTE: One might need to clear the game state before

        std::uint16_t s = 0;
        std::uint16_t nchunk = save_chunks_.size();
        for (auto& c : save_chunks_) {
            net_.send_message(netcom::all_actor_id, make_packet<message::server::game_load_progress>(
                nchunk, s, c.name()
            ));

            c.deserialize(dir);
            c.load_data_first_pass();

            ++s;
        }

        net_.send_message(netcom::all_actor_id, make_packet<message::server::game_load_progress>(
            nchunk, nchunk, "loading_second_pass"
        ));

        // A two pass loading is needed for some components
        for (auto& c : save_chunks_) {
            c.load_data_second_pass();
        }

        // Clear buffers
        for (auto& c : save_chunks_) {
            c.clear();
        }
    }

    bool game::is_saved_game_directory(const std::string& dir) const {
        for (auto& c : save_chunks_) {
            if (!c.is_valid_directory(dir)) {
                return false;
            }
        }

        return true;
    }
}
}
