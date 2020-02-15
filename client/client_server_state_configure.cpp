#include "client_server_state_configure.hpp"
#include "client_server_state_game.hpp"
#include "client_server_instance.hpp"
#include <string.hpp>
#include <sol.hpp>

namespace client {
namespace server_state {
    configure::configure(server_instance& serv) :
        server_state::base(serv, server::state_id::configure, "configure"),
        config_(net_, client::netcom::server_actor_id, "server_state_configure"),
        generator_config_(net_, client::netcom::server_actor_id, "server_state_configure_generator") {

        plist_ = std::make_unique<client::player_list>(serv_);
        plist_->connect();

        pool_ << net_.watch_message([this](const message::server::configure_current_generator_changed& msg) {
            generator_ = msg.gen;
            on_generator_changed.dispatch(generator_);
            serv_.on_debug_message.dispatch("new generator set: "+msg.gen);
        });

        pool_ << net_.watch_message([this](const message::server::configure_generating&) {
            on_generating_started.dispatch();
            serv_.on_debug_message.dispatch("started generating...");
        });

        pool_ << net_.watch_message([this](const message::server::configure_generated& msg) {
            if (msg.failed) {
                on_generating_failure.dispatch(msg.reason);
                serv_.on_debug_error.dispatch("generating failed, reason: "+msg.reason);
            } else {
                on_generating_success.dispatch();
                serv_.on_debug_message.dispatch("generated successfuly");
            }
        });

        pool_ << net_.watch_message([this](const message::server::configure_loading&) {
            on_loading_started.dispatch();
            serv_.on_debug_message.dispatch("started loading...");
        });

        pool_ << net_.watch_message([this](const message::server::configure_loaded& msg) {
            if (msg.failed) {
                on_loading_failure.dispatch(msg.reason);
                serv_.on_debug_error.dispatch("loading failed, reason: "+msg.reason);
            } else {
                on_loading_success.dispatch();
                serv_.on_debug_message.dispatch("loaded successfuly");
            }
        });
    }

    void configure::transition_to_(server_state::base& st) {
        if (st.id() == server::state_id::game) {
            server_state::game& gs = static_cast<server_state::game&>(st);
            gs.set_player_list(std::move(plist_));
        }
    }

    std::string configure::get_current_generator() const {
        std::string gen;
        config_.get_value("generator", gen);
        return gen;
    }

    void configure::register_lua(sol::state& lua) {
        auto px = lua["server"];
        if (px.is<sol::nil_t>()) {
            lua.create_table("server");
        }

        auto stbl = px.get<sol::table>();
        plist_->register_lua(stbl);

        auto ctbl = stbl.create_table("config");
        ctbl.set_function("list_parameters", [this](std::string key) {
            sol::optional<std::vector<std::string>> ret;
            try {
                ret.set(config_.list_values(key));
            } catch (const std::exception& e) {
                serv_.on_debug_message.dispatch(e.what());
            }
            return config_.list_values(key);
        });
        ctbl.set_function("get_parameter", [this](std::string key) {
            sol::optional<std::string> ret;
            std::string val;
            if (config_.get_value(key, val)) {
                ret.set(std::move(val));
            }
            return ret;
        });
        ctbl.set_function("get_parameter_type", [this](std::string key) {
            std::string val;
            if (!config_.get_value_type(key, val)) {
                val = "string";
            }
            return val;
        });
        ctbl.set_function("get_parameter_range", [this](std::string key) {
            sol::optional<std::string> retmin, retmax;
            std::string minval, maxval;
            if (config_.get_value_min(key, minval)) {
                retmin.set(minval);
            }
            if (config_.get_value_max(key, maxval)) {
                retmax.set(maxval);
            }
            return std::make_tuple(retmin, retmax);
        });
        ctbl.set_function("get_parameter_allowed_values", [this](std::string key) {
            sol::optional<std::vector<std::string>> ret;
            std::vector<std::string> vals;
            if (config_.get_value_allowed(key, vals)) {
                ret.set(std::move(vals));
            }
            return ret;
        });

        ctbl.set_function("set_parameter", [this](std::string key, std::string value) {
            pool_ << net_.send_request(client::netcom::server_actor_id,
                make_packet<request::server::configure_change_parameter>(key, value),
                [this,key,value](const client::netcom::request_answer_t<request::server::configure_change_parameter>& msg) {
                    if (msg.failed) {
                        switch (msg.failure.rsn) {
                        case request::server::configure_change_parameter::failure::reason::no_such_parameter:
                            serv_.on_debug_error.dispatch("no server configuration parameter '"+key+"'");
                            break;
                        case request::server::configure_change_parameter::failure::reason::invalid_value:
                            serv_.on_debug_error.dispatch("invalid value for '"+key+"' ('"+value+"')");
                            break;
                        }
                    }
                }
            );
        });

        ctbl.set_function("list_generator_parameters", [this](std::string key) {
            sol::optional<std::vector<std::string>> ret;
            try {
                ret.set(generator_config_.list_values(key));
            } catch (const std::exception& e) {
                serv_.on_debug_message.dispatch(e.what());
            }
            return ret;
        });
        ctbl.set_function("get_generator_parameter", [this](std::string key) {
            sol::optional<std::string> ret;
            std::string val;
            if (generator_config_.get_value(key, val)) {
                ret.set(std::move(val));
            }
            return ret;
        });
        ctbl.set_function("get_generator_parameter_type", [this](std::string key) {
            std::string val;
            if (!generator_config_.get_value_type(key, val)) {
                val = "string";
            }
            return val;
        });
        ctbl.set_function("get_generator_parameter_range", [this](std::string key) {
            sol::optional<std::string> retmin, retmax;
            std::string minval, maxval;
            if (generator_config_.get_value_min(key, minval)) {
                retmin.set(minval);
            }
            if (generator_config_.get_value_max(key, maxval)) {
                retmax.set(maxval);
            }
            return std::make_tuple(retmin, retmax);
        });
        ctbl.set_function("get_generator_parameter_allowed_values", [this](std::string key) {
            sol::optional<std::vector<std::string>> ret;
            std::vector<std::string> vals;
            if (generator_config_.get_value_allowed(key, vals)) {
                ret.set(std::move(vals));
            }
            return ret;
        });

        ctbl.set_function("set_generator_parameter", [this](std::string key, std::string value) {
            pool_ << net_.send_request(client::netcom::server_actor_id,
                make_packet<request::server::configure_change_generator_parameter>(key, value),
                [this](const client::netcom::request_answer_t<request::server::configure_change_generator_parameter>& msg) {}
            );
        });

        ctbl.set_function("generate", [this]() {
            pool_ << net_.send_request(client::netcom::server_actor_id,
                make_packet<request::server::configure_generate>(),
                [this](const client::netcom::request_answer_t<request::server::configure_generate>& msg) {
                    using failure = request::server::configure_generate::failure;
                    if (msg.failed) {
                        switch (msg.failure.rsn) {
                        case failure::reason::no_generator_set:
                            serv_.on_debug_error.dispatch("cannot generate, no generator set");
                            break;
                        case failure::reason::invalid_generator:
                            serv_.on_debug_error.dispatch("cannot generate, invalid generator");
                            break;
                        case failure::reason::already_generating:
                            serv_.on_debug_error.dispatch("cannot generate, already generating");
                            break;
                        case failure::reason::cannot_generate_while_loading:
                            serv_.on_debug_error.dispatch("cannot generate while loading");
                            break;
                        }

                        if (!msg.failure.details.empty()) {
                            serv_.on_debug_error.dispatch(msg.failure.details);
                        }
                    }
                }
            );
        });

        ctbl.set_function("run_game", [this]() {
            pool_ << net_.send_request(client::netcom::server_actor_id,
                make_packet<request::server::configure_run_game>(),
                [this](const client::netcom::request_answer_t<request::server::configure_run_game>& msg) {
                    using failure = request::server::configure_run_game::failure;

                    if (msg.failed) {
                        switch (msg.failure.rsn) {
                        case failure::reason::cannot_run_while_generating:
                            serv_.on_debug_error.dispatch("cannot run game, generating in progress");
                            break;
                        case failure::reason::cannot_run_while_loading:
                            serv_.on_debug_error.dispatch("cannot run game, loading in progress");
                            break;
                        case failure::reason::no_game_loaded:
                            serv_.on_debug_error.dispatch("cannot run game, no game loaded");
                            break;
                        }

                        if (!msg.failure.details.empty()) {
                            serv_.on_debug_error.dispatch(msg.failure.details);
                        }
                    }
                }
            );
        });
    }

    void configure::unregister_lua(sol::state& lua) {
        auto tbl = lua["server"].get<sol::table>();
        plist_->unregister_lua(tbl);
        tbl["config"] = sol::nil;
    }
}
}
