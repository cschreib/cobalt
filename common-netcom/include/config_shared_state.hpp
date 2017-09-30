#ifndef CONFIG_SHARED_STATE_HPP
#define CONFIG_SHARED_STATE_HPP

#include "shared_collection.hpp"
#include "netcom_base.hpp"

namespace packet {
    NETCOM_PACKET(config_state) {
        std::string serialized;
    };

    NETCOM_PACKET(config_value_changed) {
        std::string name;
        std::string value;
    };

    NETCOM_PACKET(config_empty) {};

    NETCOM_PACKET(config_clear) {};
}

namespace config {
    extern const std::string meta_header;

    class typed_state : private state {
        template<typename T>
        bool set_value_(const std::string& name, const T& value) {
            std::vector<T> allowed_vals;
            if (get_value_allowed(name, allowed_vals)) {
                bool found = false;
                for (auto& a : allowed_vals) {
                    if (value == a) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    return false;
                }
            } else {
                T min_val;
                if (get_value_min(name, min_val) && value < min_val) {
                    return false;
                }

                T max_val;
                if (get_value_max(name, max_val) && value > max_val) {
                    return false;
                }
            }

            state::set_value(name, value);
            return true;
        }

        template<typename T>
        bool check_type_(const std::string& type, const T& value, std::true_type) {
            if (std::is_floating_point<T>::value) {
                if (type == "float") {
                    return true;
                } else {
                    return false;
                }
            }

            if (std::is_signed<T>::value) {
                if (type == "int" || (type == "uint" && value >= 0)) {
                    return true;
                } else {
                    return false;
                }
            }

            if (std::is_unsigned<T>::value) {
                if (type == "int" || type == "uint") {
                    return true;
                } else {
                    return false;
                }
            }

            if (type == "string") {
                return true;
            } else {
                return false;
            }
        }

        template<typename T>
        bool check_type_(const std::string& type, const T& value, std::false_type) {
            if (type == "string") {
                return true;
            } else {
                return false;
            }
        }

        template<typename T>
        bool check_type_(const std::string& type, const T& value) {
            return check_type_(type, value, std::is_arithmetic<T>{});
        }

        bool is_meta(const std::string& name) const;

    public :
        template<typename T>
        bool set_value(const std::string& name, const T& value) {
            if (is_meta(name)) {
                return false;
            }

            std::string type;
            if (!get_value_type(name, type)) {
                return set_value_(name, value);
            }

            if (check_type_(type, value)) {
                return set_value_(name, value);
            } else {
                return false;
            }
        }

        bool set_raw_value(const std::string& name, std::string value) {
            if (is_meta(name)) {
                return false;
            }

            std::string type;
            if (!get_value_type(name, type)) {
                type = "string";
            }

            if (type == "string") {
                return set_value_(name, value);
            } else if (type == "int") {
                int ival;
                if (!string::stringify<int>::parse(ival, value)) {
                    return false;
                }

                return set_value_(name, ival);
            } else if (type == "uint") {
                unsigned int ival;
                if (!string::stringify<unsigned int>::parse(ival, value)) {
                    return false;
                }

                return set_value_(name, ival);
            } else if (type == "float") {
                float fval;
                if (!string::stringify<float>::parse(fval, value)) {
                    return false;
                }

                return set_value_(name, fval);
            } else {
                return false;
            }
        }

        bool get_value_type(const std::string& name, std::string& type) {
            return state::get_value(meta_header+name+".type", type);
        }

        template<typename T>
        bool get_value_min(const std::string& name, T& min_val) {
            return state::get_value(meta_header+name+".min_value", min_val);
        }

        template<typename T>
        bool get_value_max(const std::string& name, T& max_val) {
            return state::get_value(meta_header+name+".max_value", max_val);
        }

        template<typename T>
        bool get_value_range(const std::string& name, T& min_val, T& max_val) {
            return state::get_value(meta_header+name+".min_value", min_val) &&
                   state::get_value(meta_header+name+".max_value", max_val);
        }

        template<typename T>
        bool get_value_allowed(const std::string& name, std::vector<T>& vals) {
            return state::get_value(meta_header+name+".allowed_values", vals);
        }

        void set_value_type(const std::string& name, std::string type) {
            state::set_value(meta_header+name+".type", type);
        }

        template<typename T>
        bool set_value_min(const std::string& name, const T& min_val) {
            std::string type;
            if (!get_value_type(name, type) || check_type_(type, min_val)) {
                state::set_value(meta_header+name+".min_value", min_val);
                return true;
            } else {
                return false;
            }
        }

        template<typename T>
        bool set_value_max(const std::string& name, const T& max_val) {
            std::string type;
            if (!get_value_type(name, type) || check_type_(type, max_val)) {
                state::set_value(meta_header+name+".max_value", max_val);
                return true;
            } else {
                return false;
            }
        }

        template<typename T>
        bool set_value_range(const std::string& name, const T& min_val, const T& max_val) {
            std::string type;
            if (!get_value_type(name, type) ||
                (check_type_(type, min_val) && check_type_(type, max_val))) {
                state::set_value(meta_header+name+".min_value", min_val);
                state::set_value(meta_header+name+".max_value", max_val);
                return true;
            } else {
                return false;
            }
        }

        template<typename T>
        bool set_value_allowed(const std::string& name, const std::vector<T>& vals) {
            std::string type;
            if (!get_value_type(name, type)) {
                state::set_value(meta_header+name+".allowed_values", vals);
                return true;
            } else {
                for (auto& v : vals) {
                    if (!check_type_(type, v)) {
                        return false;
                    }
                }

                state::set_value(meta_header+name+".allowed_values", vals);
                return true;
            }
        }

        using state::on_value_changed;
        using state::parse;
        using state::parse_from_file;
        using state::save;
        using state::save_to_file;
        using state::get_value;
        using state::get_raw_value;
        using state::value_exists;
        using state::bind;
        using state::clear;
    };

    class shared_state : private typed_state {
        struct shared_collection_traits {
            using full_packet   = packet::config_state;
            using add_packet    = packet::config_value_changed;
            using remove_packet = packet::config_empty;
            using clear_packet  = packet::config_clear;
        };

        friend class shared_state_observer;

        shared_collection<shared_collection_traits> shared_;

    public :
        template<typename Netcom>
        shared_state(Netcom& net, std::string name) :
            shared_(net.template make_shared_collection<shared_collection_traits>(
                std::move(name))) {
            shared_.make_collection_packet([this](packet::config_state& st) {
                std::ostringstream ss;
                save(ss);
                st.serialized = ss.str();
            });

            on_value_changed.connect([this](const std::string& name, const std::string& value) {
                shared_.add_item(name, value);
            });

            shared_.connect();
        }

        void clear();

        using typed_state::on_value_changed;
        using typed_state::parse;
        using typed_state::parse_from_file;
        using typed_state::save;
        using typed_state::save_to_file;
        using typed_state::get_value;
        using typed_state::get_raw_value;
        using typed_state::set_value;
        using typed_state::set_raw_value;
        using typed_state::set_value_type;
        using typed_state::set_value_min;
        using typed_state::set_value_max;
        using typed_state::set_value_range;
        using typed_state::set_value_allowed;
        using typed_state::get_value_type;
        using typed_state::get_value_min;
        using typed_state::get_value_max;
        using typed_state::get_value_range;
        using typed_state::get_value_allowed;
        using typed_state::value_exists;
        using typed_state::bind;
    };

    class shared_state_observer : private state {
        shared_collection_observer<shared_state::shared_collection_traits> shared_;
        scoped_connection_pool pool_;
        actor_id_t aid_;

    public :
        template<typename Netcom>
        shared_state_observer(Netcom& net, actor_id_t aid, const std::string& name) : aid_(aid) {
            pool_ << net.send_request(aid, make_packet<request::get_shared_collection_id>(name),
                [&,this](const netcom_base::request_answer_t<request::get_shared_collection_id>& msg) {

                shared_ = net.template make_shared_collection_observer
                    <shared_state::shared_collection_traits>(msg.answer.id);

                shared_.on_received.connect([this](const packet::config_state& st) {
                    std::istringstream ss(st.serialized);
                    parse(ss);
                });

                shared_.on_add_item.connect([this](const packet::config_value_changed& val) {
                    set_raw_value(val.name, val.value);
                });

                shared_.on_clear.connect([this](const packet::config_clear& val) {
                    clear();
                });

                shared_.connect(aid_);
            });
        }

        using state::on_value_changed;
        using state::save;
        using state::save_to_file;
        using state::get_value;
        using state::get_raw_value;
        using state::value_exists;
        using state::bind;
    };
}

#ifndef NO_AUTOGEN
#include "autogen/packets/config_shared_state.hpp"
#endif

#endif
