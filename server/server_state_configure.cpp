#include "server_state_configure.hpp"
#include "server_state_game.hpp"
#include "server_state_idle.hpp"
#include "server_instance.hpp"
#include <filesystem.hpp>
#include <time.hpp>

namespace server {
namespace state {
    configure::configure(server::instance& serv) :
        base(serv, server::state_id::configure, "configure"),
        config_(net_, "server_state_configure") {

        update_generator_list();

        plist_ = std::make_unique<server::player_list>(net_, serv_.get_conf());

        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::configure_list_generators>&& req) {
            req.answer(available_generators_);
        });

        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::configure_get_current_generator>&& req) {
            req.answer(generator_->id);
        });

        rw_pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::configure_set_current_generator>&& req) {
            if (set_generator(req.arg.gen)) {
                req.answer();
            } else {
                using failure_t = request::server::configure_set_current_generator::failure;
                req.fail(failure_t::reason::no_such_generator);
            }
        });

        rw_pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::configure_generate>&& req) {
            try {
                generate();
                req.answer();
            } catch (request::server::configure_generate::failure& fail) {
                req.fail(std::move(fail));
            } catch (...) {
                out_.error("unexpected exception in configure::generate()");
                throw;
            }
        });

        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::configure_list_saved_games>&& req) {
            req.answer(saved_games_);
        });

        pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::configure_is_game_loaded>&& req) {
            req.answer(!loading_ && loaded_game_);
        });

        rw_pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::configure_load_game>&& req) {
            try {
                load_saved_game(req.arg.save, false);
                req.answer();
            } catch (request::server::configure_load_game::failure& fail) {
                req.fail(std::move(fail));
            } catch (...) {
                out_.error("unexpected exception in configure::load_saved_game()");
                throw;
            }
        });

        rw_pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::configure_run_game>&& req) {
            try {
                run_game();
                req.answer();
            } catch (request::server::configure_run_game::failure& fail) {
                req.fail(std::move(fail));
            } catch (...) {
                out_.error("unexpected exception in configure::run_game()");
                throw;
            }
        });

        rw_pool_ << net_.watch_request(
            [this](server::netcom::request_t<request::server::stop_and_idle>&& req) {
            serv_.set_state<server::state::idle>();
            req.answer();
        });
    }

    configure::~configure() {
        if (thread_.joinable()) thread_.join();
    }

    void configure::update_saved_game_list() {
        saved_games_.clear();

        auto saves = file::list_directories("saves/");
        auto iter = std::find(saves.begin(), saves.end(), "generated");
        if (iter != saves.end()) {
            saves.erase(iter);
            auto gens = file::list_directories("saves/generated/");
            for (auto& s : gens) {
                saved_games_.insert("generated/"+s);
            }
        }

        for (auto& s : saves) {
            saved_games_.insert(s);
        }
    }

    void configure::update_generator_list() {
        available_generators_.clear();

        auto libs = file::list_files("generators/*."+shared_library::file_extension);
        for (auto&& lib_file : libs) {
            shared_library lib("generators/"+lib_file);
            if (lib.open() && lib.load_symbol("generate_universe") != nullptr) {
                available_generators_.insert(generator_info{
                    file::remove_extension(lib_file), lib_file
                });
            }
        }

        if (available_generators_.empty()) {
            out_.warning("no universe generator available");
        } else {
            set_generator(available_generators_.front().id);
        }
    }

    bool configure::set_generator(const std::string& id) {
        auto iter = available_generators_.find(id);
        if (iter == available_generators_.end()) return false;

        generator_ = &*iter;
        config_.clear();
        config_.parse_from_file("generators/"+generator_->id+".conf");
        config_.set_value("generator", generator_->id);

        return true;
    }

    void configure::generate() {
        using failure = request::server::configure_generate::failure;

        if (generating_) {
            throw failure{failure::reason::already_generating, ""};
        }

        if (loading_) {
            throw failure{failure::reason::cannot_generate_while_loading, ""};
        }

        if (!generator_) {
            throw failure{failure::reason::no_generator_set, ""};
        }

        // Load generator
        std::string lib_file = "generators/"+generator_->libfile;
        if (!file::exists(lib_file)) {
            throw failure{failure::reason::invalid_generator,
                "file could not be found"};
        }

        shared_library lib(lib_file);
        if (!lib.open()) {
            throw failure{failure::reason::invalid_generator,
                "file is not a dynamic library"};
        }

        auto* generate_universe = lib.load_function<bool(const char*, char**)>("generate_universe");
        if (!generate_universe) {
            throw failure{failure::reason::invalid_generator,
                "library does not contain the 'generate_universe' function"};
        }


        auto* free_error = lib.load_function<void(char*)>("free_error");
        if (!free_error) {
            throw failure{failure::reason::invalid_generator,
                "library does not contain the 'free_error' function"};
        }

        // Make directory
        if (save_dir_.empty()) {
            save_dir_ = "saves/generated/"+generator_->id+"-"+today_str()+time_of_day_str()+"/";
            file::mkdir(save_dir_);
        }

        config_.set_value("output_directory", save_dir_);

        // Serialize config
        std::string serialized_config; {
            std::ostringstream ss;
            config_.save(ss);
            serialized_config = ss.str();
        }

        // Save config
        {
            std::ofstream file(save_dir_+"generator.conf");
            file << serialized_config;
        }

        // Launch generation
        rw_pool_.block_all();
        net_.send_message<message::server::configure_generating>(server::netcom::all_actor_id);
        generating_ = true;

        // Register callback for end of generation
        pool_ << net_.watch_message<watch_policy::once>(
            [this](message::server::configure_generated_internal msg) {

            generating_ = false;

            if (!msg.failed) {
                load_generated_saved_game_();
            } else {
                message::server::configure_generated out_msg;
                out_msg.failed = true; out_msg.reason = msg.reason;
                net_.send_message(server::netcom::all_actor_id, std::move(out_msg));
                rw_pool_.unblock_all();
            }
        });

        // Do the actual work in a thread
        if (thread_.joinable()) thread_.join();
        thread_ = std::thread([this, serialized_config, generate_universe, free_error](shared_library) {
            char* errmsg = nullptr;
            bool ret = false;

            // Use a scoped lambda to make this exception safe
            auto scoped = ctl::make_scoped([&]() {
                message::server::configure_generated_internal msg;
                msg.failed = !ret;
                if (!ret) {
                    if (errmsg != nullptr) {
                        msg.reason = errmsg;
                    } else {
                        msg.reason = "unknown";
                    }
                }

                net_.send_message(server::netcom::self_actor_id, msg);

                if (errmsg != nullptr) {
                    (*free_error)(errmsg);
                }
            });

            ret = (*generate_universe)(serialized_config.c_str(), &errmsg);
        }, std::move(lib));
    }

    void configure::load_generated_saved_game_() {
        using failure = request::server::configure_load_game::failure;

        // Register callback for end of loading
        pool_ << net_.watch_message<watch_policy::once>(
            [this](const message::server::configure_loaded_internal& msg) {

            message::server::configure_generated out_msg;
            out_msg.failed = msg.failed;
            if (msg.failed) {
                out_msg.reason = "loading of generated universe failed";
            }

            net_.send_message(server::netcom::all_actor_id, std::move(out_msg));
        });

        // Try loading
        try {
            load_saved_game(save_dir_, true);
        } catch (failure& fail) {
            message::server::configure_generated out_msg;
            out_msg.failed = true;
            switch (fail.rsn) {
                case failure::reason::invalid_saved_game :
                    out_msg.reason = "the generated save file is invalid"; break;
                case failure::reason::no_such_saved_game :
                case failure::reason::already_loading :
                case failure::reason::cannot_load_while_generating :
                    // Will never happen
                    out_msg.reason = "unexpected code path: logic error while calling "
                        "load_saved_game inside generate";
                    out_.error(out_msg.reason);
                    break;
            }

            net_.send_message(server::netcom::all_actor_id, std::move(out_msg));
            rw_pool_.unblock_all();
        } catch (...) {
            rw_pool_.unblock_all();
            throw;
        }
    }

    void configure::load_saved_game(const std::string& dir, bool just_generated) {
        using failure = request::server::configure_load_game::failure;

        if (loading_) {
            throw failure{failure::reason::already_loading, ""};
        }

        if (generating_) {
            throw failure{failure::reason::cannot_load_while_generating, ""};
        }

        if (!file::exists(dir)) {
            throw failure{failure::reason::no_such_saved_game, ""};
        }

        loaded_game_ = std::make_unique<state::game>(serv_);

        if (!loaded_game_->is_saved_game_directory(dir)) {
            throw failure{failure::reason::invalid_saved_game, ""};
        }

        rw_pool_.block_all();

        if (!just_generated) {
            // Import the configuration of this previously saved game.
            // This is not needed if this function is called by generate()
            // since the corresponding configuration is already loaded.
            config_.clear();
            config_.parse_from_file(dir+"generator.conf");

            std::string gid;
            config_.get_value("generator", gid);

            auto iter = available_generators_.find(gid);
            if (iter == available_generators_.end()) {
                generator_ = nullptr;
                out_.warning("loading a saved game generated from an unknown generator '",
                    gid, "'");
            } else {
                generator_ = &*iter;
            }
        }

        loading_ = true;
        net_.send_message<message::server::configure_loading>(server::netcom::all_actor_id);

        // Register callback for end of loading
        pool_ << net_.watch_message<watch_policy::once>(
            [this](const message::server::configure_loaded_internal& msg) {

            loading_ = false;
            rw_pool_.unblock_all();

            message::server::configure_loaded out_msg;
            out_msg.failed = msg.failed; out_msg.reason = msg.reason;
            net_.send_message(server::netcom::all_actor_id, std::move(out_msg));
        });

        // Do the actual work in a thread
        if (thread_.joinable()) thread_.join();
        thread_ = std::thread([this, dir]() {
            message::server::configure_loaded_internal msg;
            msg.failed = false;

            try {
                loaded_game_->load_from_directory(dir);
            } catch (request::server::game_load::failure& fail) {
                msg.failed = true;
                msg.reason = fail.details;
            } catch (...) {
                msg.failed = true;
                msg.reason = "unknown";
            }

            net_.send_message(server::netcom::self_actor_id, msg);
        });
    }

    void configure::run_game() {
        using failure = request::server::configure_run_game::failure;

        if (generating_) {
            throw failure{failure::reason::cannot_run_while_generating, ""};
        }

        if (loading_) {
            throw failure{failure::reason::cannot_run_while_loading, ""};
        }

        if (!loaded_game_) {
            throw failure{failure::reason::no_game_loaded, ""};
        }

        loaded_game_->set_player_list(std::move(plist_));
        serv_.set_state(std::move(loaded_game_));
    }
}
}
