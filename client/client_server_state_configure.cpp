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

        plist_ = std::make_unique<client::player_list>(net_);

        pool_ << net_.watch_message([this](const message::server::configure_current_generator_changed& msg) {
            generator_ = msg.gen;
            on_generator_changed.dispatch(generator_);
            serv_.on_debug_message.dispatch("new generator set: "+msg.gen);
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
                            serv_.on_debug_message.dispatch("error: no server configuration parameter '"+key+"'");
                            break;
                        case request::server::configure_change_parameter::failure::reason::invalid_value:
                            serv_.on_debug_message.dispatch("error: invalid value for '"+key+"' ('"+value+"')");
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
    }

    void configure::unregister_lua(sol::state& lua) {
        auto tbl = lua["server"].get<sol::table>();

        tbl["config"] = sol::nil;
    }
}
}
