#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "string_tree.hpp"
#include "stringify.hpp"
#include "signal.hpp"
#include <sstream>

namespace config {
    namespace impl {
        template<typename T>
        using is_configurable = std::integral_constant<bool,
            ctl::has_left_shift<std::ostream, T>::value &&
            ctl::has_right_shift<std::istream, T>::value
        >;
    }

    /// Holds all the configurable parameters of the game.
    /** The configurable parameters are arranged in a tree, for clarity and (hopefully) faster
        traversal. For example :

            graphics
                resolution
                fullscreen
                vsync
                window
                    dimensions
                    position
            ui
                textsize
                locale

        In this tree, "graphics", "window" and "ui" are branches, in the sense that they do not
        contain any value by themselves, but lead to other parameters. All the other items are
        called "leaves" : they hold a single parameter value.

        Also note that, for a new parameter to be created, its associated C++ type T must be
        "configurable", that is there must exist a specialization of string::stringify for T. This
        specialization must provide two static functions :

        \code{.cpp}
            bool string::stringify<T>::parse(T& t, const std::string& value);
            void string::stringify<T>::serialize(const T& t, std::string& value);
        \endcode

        "parse" tries to read out a value from "value" (it must return false if it fails to do so)
        and store it in t, and "parse" does the opposite. There is a default implementation that
        uses string conversion via standard string streams, so if a type provides both the >> and <<
        iostream operators, it is "configurable" by default.
    **/
    class state {
        struct config_node {
            std::string value;
            bool        is_empty = true;
            signal_t<void(const std::string&)> signal;
        };

        using tree_t = ctl::string_tree<config_node>;

        tree_t tree_;
        mutable bool dirty_;

    public :
        struct exception : public std::exception {
            explicit exception(const std::string& message) : message(message) {}
            const char* what() const noexcept override { return message.c_str(); }
            std::string message;
        };
        struct parsing_failure : public exception {
            explicit parsing_failure(const std::string& message) : exception(message) {}
        };

        /// Default constructor.
        /** Constructs an empty configuration state, no data is loaded.
        **/
        state();

        state(const state&) = delete;
        state(state&&) = default;
        state& operator= (const state&) = delete;
        state& operator= (state&&) = default;

        signal_t<void(const std::string&, const std::string&)> on_value_changed;

        /// Read a plain text configuration file and load its values.
        /** The configuration file is a succession of lines, each containing a single configuration
            value. A line is made of:
             - the name of the parameter (ex: "graphics.resolution")
             - the value of the parameter enclosed in parenthesis (ex: "(800, 600)")
        **/
        void parse(std::istream& flux);
        void parse_from_file(const std::string& file);

        /// Save the current configuration in a plain text file.
        /** If any, all the content of this file is erased before the new configuration is written.
            For more information on the file format, see parse().
        **/
        void save(std::ostream& flux) const;
        void save_to_file(const std::string& file) const;

        /// Remove all configuration items and bonds.
        void clear();

        /// Set the value of a parameter.
        /** If the parameter already exists, then its value is updated and all bound objects are
            notified of the change. Else, the parameter is created. This function will throw if the
            current tree structure is not compatible with the provided parameter name, or if an
            exception is raised by any of the callbacks registered to this particular parameter.
            In all cases where an exception is thrown, the value of the parameter is left unchanged.
        **/
        template<typename T>
        void set_value(const std::string& name, const T& value) {
            std::string string_value;
            string::stringify<T>::serialize(value, string_value);
            set_raw_value(name, std::move(string_value));
        }

        /// Set the value of a parameter as a string.
        /** If the parameter already exists, then its value is updated and all bound objects are
            notified of the change. Else, the parameter is created. This function will throw if
            the current tree structure is not compatible with the provided parameter name, or if an
            exception is raised by any of the callbacks registered to this particular parameter.
            In all cases where an exception is thrown, the value of the parameter is left unchanged.
            Compared to set_value(), this version takes directly a string for the new value of the
            parameter, which is stored untouched into the configuration tree.
        **/
        void set_raw_value(const std::string& name, std::string value) {
            config_node& node = tree_.reach(name);
            set_raw_value_(node, name, std::move(value));
        }

        /// Retrieve the value of a parameter from this configuration state.
        /** If the parameter exists, then its value is written in "value", and 'true' is returned.
            Else 'false' is returned and "value" is left untouched. This function will throw an
            exception if the parameter value cannot be serialized into a type T.
        **/
        template<typename T>
        bool get_value(const std::string& name, T& value) const {
            const config_node* node = tree_.try_reach(name);
            if (!node || node->is_empty) {
                return false;
            }

            if (!string::stringify<T>::parse(value, node->value)) {
                throw parsing_failure("could not parse '"+name+"' from value '"+node->value+"'");
            }

            return true;
        }

        /// Retrieve the value of a parameter from this configuration state as a string.
        /** If the parameter exists, then its value is written in "value", and 'true' is returned.
            Else 'false' is returned and "value" is left untouched. Compared to get_value(), this
            function will always retrieve the value as a string, whatever the real underlying type
            is.
        **/
        template<typename T>
        bool get_raw_value(const std::string& name, std::string& value) const {
            const config_node* node = tree_.try_reach(name);
            if (!node || node->is_empty) {
                return false;
            }

            value = node->value;
            return true;
        }

        /// Retrieve a the value from this configuration state with a default value.
        /** If the parameter exists, then its value is parsed into "value". Else, the default value
            is stored in the configuration tree, and parsed back into "value". This function will
            throw if the parsing to T fails, or if the tree structure is incompatible with the
            provided parameter name, or if setting the default value raises an exception in one of
            the registered callbacks
        **/
        template<typename T, typename N>
        void get_value(const std::string& name, T& value, const N& def) {
            config_node& node = tree_.reach(name);
            if (node.is_empty) {
                std::string string_value;
                string::stringify<N>::serialize(def, string_value);
                set_raw_value_(node, name, std::move(string_value));
            }

            if (!string::stringify<T>::parse(value, node.value)) {
                throw parsing_failure("could not parse '"+name+"' from value '"+node.value+"'");
            }
        }

        /// Check if a parameter exists.
        bool value_exists(const std::string& name) const {
            return tree_.try_reach(name) != nullptr;
        }

        /// Return the list of children values in a given root key.
        /** Will throw an exception if the requested root key does not exist.
        **/
        std::vector<std::string> list_values(const std::string& name = "") const {
            std::vector<std::string> ret;

            const auto* branch = (name == "" ? &tree_.root() : tree_.try_reach_branch(name));

            if (branch == nullptr) {
                throw tree_t::expecting_branch_exception(name);
            } else {
                ret.reserve(branch->children.size());
                for (const auto& c : branch->children) {
                    ret.push_back(c->name);
                }
            }

            return ret;
        }

        /// Bind a variable to a configurable parameter.
        /** Using this method, one can bind a C++ variable to a parameter in this configuration
            state. When the value of the parameter changes, this variable is automatically updated.
            However, if parsing the string value of the parameter into a type T is not successful,
            an exception will be thrown, and the value will be rejected. If, when calling this
            function, the parameter already contains a value and it cannot be parsed into a type T,
            the binding will not be preserved. This function will throw if the current tree
            structure is not compatible with the provided parameter name.
        **/
        template<typename T>
        signal_connection_base& bind(const std::string& name, T& var) {
            static_assert(impl::is_configurable<T>::value,
                "bound variable must be configurable");

            config_node& node = tree_.reach(name);

            auto parse_to_var = [&var,name](const std::string& value) {
                if (!string::stringify<T>::parse(var, value)) {
                    throw parsing_failure("could not parse '"+name+"' from value '"+value+"'");
                }
            };

            signal_connection_base& sc = node.signal.connect(parse_to_var);

            if (node.is_empty) {
                // Parameter does not yet have a value, set it from variable we just bound
                // and trigger signals
                std::string string_value;
                string::stringify<T>::serialize(var, string_value);
                set_raw_value_(node, name, std::move(string_value));
            } else {
                try {
                    // Parameter has a value, set the variable to this value
                    // (but no need to trigger the signal as the parameter has not changed)
                    parse_to_var(node.value);
                } catch (...) {
                    // Cancel the binding on exception
                    sc.stop();
                    throw;
                }
            }

            return sc;
        }

        /// Bind a callback function to a configurable parameter.
        /** This method can be used to register a callback function that will be executed each time
            the bound parameter is modified. This callback function is a lambda or functor, and can
            only take a single configurable argument. The lambda is allowed to throw, which will
            be interpreted as an "invalid value" signal; any parameter value which triggers an
            exception will not be accepted inside the configuration. If the parameter currently has
            a value, and this value is "invalid" (lambda throws), the binding will not be preserved.
            This function will throw if the  current tree structure is not compatible with the
            provided parameter name.
        **/
        template<typename F, typename enable =
            typename std::enable_if<!impl::is_configurable<F>::value>::type>
        signal_connection_base& bind(const std::string& name, F&& func) {
            static_assert(ctl::argument_count<F>::value == 1,
                "configuration callback can only take one argument");
            using ArgType = typename std::decay<ctl::function_argument<F>>::type;
            static_assert(impl::is_configurable<ArgType>::value,
                "configuration callback argument must be configurable");

            config_node& node = tree_.reach(name);

            auto parse_to_callback = [func,name](const std::string& value) {
                ArgType t;
                if (string::stringify<ArgType>::parse(t, value)) {
                    func(t);
                } else {
                    throw parsing_failure("could not parse '"+name+"' from value '"+value+"'");
                }
            };

            signal_connection_base& sc = node.signal.connect(parse_to_callback);

            if (!node.is_empty) {
                try {
                    // Parameter has a value, send it to the callback
                    // (but no need to trigger the signal as the parameter has not changed)
                    parse_to_callback(node.value);
                } catch (...) {
                    // Cancel the binding on exception
                    sc.stop();
                    throw;
                }
            }

            return sc;
        }

        /// Bind a callback function to a configurable parameter with a default value.
        /** See the other bind() methods for a detailed description. This overloaded version also
            takes a default value that will be saved then fed to the callback function if the
            parameter does not yet exist.
        **/
        template<typename F, typename N, typename enable =
            typename std::enable_if<!impl::is_configurable<F>::value>::type>
        signal_connection_base& bind(const std::string& name, F&& func, const N& def) {
            static_assert(ctl::argument_count<F>::value == 1,
                "configuration callback can only take one argument");
            using ArgType = typename std::decay<ctl::function_argument<F>>::type;
            static_assert(impl::is_configurable<ArgType>::value,
                "configuration callback argument must be configurable");

            config_node& node = tree_.reach(name);

            auto parse_to_callback = [func,name](const std::string& value) {
                ArgType t;
                if (string::stringify<ArgType>::parse(t, value)) {
                    func(t);
                } else {
                    throw parsing_failure("could not parse '"+name+"' from value '"+value+"'");
                }
            };

            signal_connection_base& sc = node.signal.connect(parse_to_callback);

            if (node.is_empty) {
                // Parameter does not yet have a value, set it from default and trigger signals
                std::string string_value;
                string::stringify<N>::serialize(def, string_value);
                set_raw_value_(node, name, std::move(string_value));
            } else {
                try {
                    // Parameter has a value, send it to the callback
                    // (but no need to trigger the signal as the parameter has not changed)
                    parse_to_callback(node.value);
                } catch (...) {
                    // Cancel the binding on exception
                    sc.stop();
                    throw;
                }
            }

            return sc;
        }

    private :

        void save_node_(std::ostream& f, const tree_t::branch& node, const std::string& name) const;

        void set_raw_value_(config_node& node, const std::string& name, std::string value) {
            try {
                on_value_changed.dispatch(name, value);
                node.signal.dispatch(value);

                // No exception thrown by now, we can commit the change to the state
                node.is_empty = false;
                node.value = value;
                dirty_ = true;
            } catch (...) {
                // Exception thrown, trigger callbacks again with old value in case some
                // callbacks had registered the change, which needs to be rolled back
                on_value_changed.dispatch(name, node.value);
                node.signal.dispatch(node.value);
                throw;
            }
        }
    };

    extern const std::string meta_header;

    /// config::state with typed parameters
    class typed_state : private state {
    public:
        struct incorrect_type : public state::exception {
            explicit incorrect_type(const std::string& name, const std::string& type) :
                state::exception("value has incorrect type for parameter '"+name+"', "
                        "which is of type '"+type+"'") {}
        };

        struct incorrect_value : public state::exception {
            explicit incorrect_value(const std::string& message) : state::exception(message) {}
        };

        struct accessing_meta_parameter : public state::exception {
            explicit accessing_meta_parameter(const std::string& name) :
                state::exception("cannot set value for meta-parameter '"+name+"'") {}
        };

    private:
        /// Set new value, checking against allowed values and ranges.
        template<typename T>
        void set_value_(const std::string& name, const T& value) {
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
                    std::string string_value;
                    string::stringify<T>::serialize(value, string_value);
                    throw incorrect_value("value '"+string_value+"' is not in list of allowed "
                        "values for parameter '"+name+"'");
                }
            } else {
                T min_val;
                if (get_value_min(name, min_val) && value < min_val) {
                    std::string string_value, string_min;
                    string::stringify<T>::serialize(value, string_value);
                    string::stringify<T>::serialize(min_val, string_min);
                    throw incorrect_value("value '"+string_value+"' is lower than minimum allowed "
                        "value of '"+string_min+"' for parameter '"+name+"'");
                }

                T max_val;
                if (get_value_max(name, max_val) && value > max_val) {
                    std::string string_value, string_max;
                    string::stringify<T>::serialize(value, string_value);
                    string::stringify<T>::serialize(max_val, string_max);
                    throw incorrect_value("value '"+string_value+"' is larger than maximum allowed "
                        "value of '"+string_max+"' for parameter '"+name+"'");
                }
            }

            state::set_value(name, value);
        }

        /// Check if new value type T (arithmetic) matches stored type.
        template<typename T>
        bool check_type_(const std::string& type, const T& value, std::true_type) {
            if (std::is_floating_point<T>::value) {
                return type == "float";
            }

            if (std::is_signed<T>::value) {
                return type == "int" || (type == "uint" && value >= 0);
            }

            if (std::is_unsigned<T>::value) {
                return type == "int" || type == "uint";
            }

            return type == "string";
        }

        /// Check if new value type T (string) matches stored type.
        template<typename T>
        bool check_type_(const std::string& type, const T& value, std::false_type) {
            return type == "string";
        }

        /// Check if new value type T matches stored type.
        template<typename T>
        bool check_type_(const std::string& type, const T& value) {
            return check_type_(type, value, std::is_arithmetic<T>{});
        }

        /// Check if a given parameter is a meta-parameter.
        bool is_meta(const std::string& name) const;

    public :

        /// Set the value of a parameter.
        /** If the parameter already exists, then its value is updated and all bound objects are
            notified of the change. Else, the parameter is created. This function will throw if the
            current tree structure is not compatible with the provided parameter name, or if the
            new value does not satisfy the type or value requirements, or if an exception is raised
            by any of the callbacks registered to this particular parameter. In all cases where
            exceptions are thrown, the value of the parameter is left unchanged.
        **/
        template<typename T>
        void set_value(const std::string& name, const T& value) {
            if (is_meta(name)) {
                throw accessing_meta_parameter(name);
            }

            std::string type;
            if (get_value_type(name, type) && !check_type_(type, value)) {
                throw incorrect_type(name, type);
            }

            set_value_(name, value);
        }

        /// Set the value of a parameter as a string.
        /** If the parameter already exists, then its value is updated and all bound objects are
            notified of the change. Else, the parameter is created. This function will throw if
            the current tree structure is not compatible with the provided parameter name, or if the
            new value does not satisfy the type or value requirements, or if an exception is raised
            by any of the callbacks registered to this particular parameter. In all cases where an
            exception is thrown, the value of the parameter is left unchanged. Compared to
            set_value(), this version takes directly a string for the new value of the parameter,
            which is stored untouched into the configuration tree.
        **/
        void set_raw_value(const std::string& name, std::string value) {
            if (is_meta(name)) {
                throw accessing_meta_parameter(name);
            }

            std::string type;
            if (!get_value_type(name, type)) {
                type = "string";
            }

            if (type == "int") {
                int ival;
                if (!string::stringify<int>::parse(ival, value)) {
                    throw parsing_failure("could not parse parameter '"+name+"' of type '"+type
                        +"' from value '"+value+"'");
                }

                set_value_(name, ival);
            } else if (type == "uint") {
                unsigned int ival;
                if (!string::stringify<unsigned int>::parse(ival, value)) {
                    throw parsing_failure("could not parse parameter '"+name+"' of type '"+type
                        +"' from value '"+value+"'");
                }

                set_value_(name, ival);
            } else if (type == "float") {
                float fval;
                if (!string::stringify<float>::parse(fval, value)) {
                    throw parsing_failure("could not parse parameter '"+name+"' of type '"+type
                        +"' from value '"+value+"'");
                }

                set_value_(name, fval);
            } else {
                set_value_(name, value);
            }
        }

        /// Get the type of a parameter
        /** Will return 'true' and store the type in the 'type' argument if a type is specified.
            Otherwise the function will return 'false', which can signify either that the parameter
            does not exist, or that it has no type.
        **/
        bool get_value_type(const std::string& name, std::string& type) {
            return state::get_value(meta_header+"."+name+".type", type);
        }

        /// Get the minimum value of a parameter.
        /** Will return 'true' and store the minimum in the 'min_val' argument if a minimum is
            specified. Otherwise the function will return 'false', which can signify either that the
            parameter does not exist, or that it has no minimum value. This function can throw if
            the minimum value cannot be parsed into a type T.
        **/
        template<typename T>
        bool get_value_min(const std::string& name, T& min_val) {
            return state::get_value(meta_header+"."+name+".min_value", min_val);
        }

        /// Get the maximum value of a parameter.
        /** Will return 'true' and store the maximum in the 'max_val' argument if a maximum is
            specified. Otherwise the function will return 'false', which can signify either that the
            parameter does not exist, or that it has no maximum value. This function can throw if
            the maximum value cannot be parsed into a type T.
        **/
        template<typename T>
        bool get_value_max(const std::string& name, T& max_val) {
            return state::get_value(meta_header+"."+name+".max_value", max_val);
        }

        /// Get the minimum and maximum values of a parameter.
        /** Will return 'true' and store the minimum and maximum in the 'min_val' and 'max_val'
            arguments if a minimum and a maximum are specified. Otherwise the function will return
            'false', which can signify either that the parameter does not exist, or that it has no
            minimum or maximum value. This function can throw if the minimum or maximum value cannot
            be parsed into a type T.
        **/
        template<typename T>
        bool get_value_range(const std::string& name, T& min_val, T& max_val) {
            return state::get_value(meta_header+"."+name+".min_value", min_val) &&
                   state::get_value(meta_header+"."+name+".max_value", max_val);
        }

        /// Get the list of allowed values of a parameter.
        /** Will return 'true' and store the list in the 'vals' argument if a list of allowed values
            is specified. Otherwise the function will return 'false', which can signify either that
            the parameter does not exist, or that it has no list of allowed value. This function can
            throw if the list cannot be parsed into a type vector<T>.
        **/
        template<typename T>
        bool get_value_allowed(const std::string& name, std::vector<T>& vals) {
            return state::get_value(meta_header+"."+name+".allowed_values", vals);
        }

        /// Set the type of a parameter.
        /** Supported types are "int", "uint", "float", and "string". Any other type will be
            interpreted as "string". This function will throw if the tree structure is incompatible
            with the required parameter name.
        **/
        void set_value_type(const std::string& name, std::string type) {
            state::set_value(meta_header+"."+name+".type", type);
        }

        /// Set the minimum value of a parameter.
        /** This function will throw if the tree structure is incompatible with the required
            parameter name, or if the provided minimum value has an incompatible type.
        **/
        template<typename T>
        void set_value_min(const std::string& name, const T& min_val) {
            std::string type;
            if (get_value_type(name, type) && !check_type_(type, min_val)) {
                throw incorrect_type(name, type);
            }

            state::set_value(meta_header+"."+name+".min_value", min_val);
        }

        /// Set the maximum value of a parameter.
        /** This function will throw if the tree structure is incompatible with the required
            parameter name, or if the provided maximum value has an incompatible type.
        **/
        template<typename T>
        void set_value_max(const std::string& name, const T& max_val) {
            std::string type;
            if (get_value_type(name, type) && !check_type_(type, max_val)) {
                throw incorrect_type(name, type);
            }

            state::set_value(meta_header+"."+name+".max_value", max_val);
        }

        /// Set the minimum and maximum values of a parameter.
        /** This function will throw if the tree structure is incompatible with the required
            parameter name, or if the provided minimum or maximum value have an incompatible type.
        **/
        template<typename T>
        void set_value_range(const std::string& name, const T& min_val, const T& max_val) {
            std::string type;
            if (get_value_type(name, type) &&
                (!check_type_(type, min_val) || !check_type_(type, max_val))) {
                throw incorrect_type(name, type);
            }

            state::set_value(meta_header+"."+name+".min_value", min_val);
            state::set_value(meta_header+"."+name+".max_value", max_val);
        }

        /// Set the list of allowed values of a parameter.
        /** This function will throw if the tree structure is incompatible with the required
            parameter name, or if any value in the provided list has an incompatible type.
        **/
        template<typename T>
        void set_value_allowed(const std::string& name, const std::vector<T>& vals) {
            std::string type;
            if (get_value_type(name, type)) {
                for (auto& v : vals) {
                    if (!check_type_(type, v)) {
                        throw incorrect_type(name, type);
                    }
                }
            }

            state::set_value(meta_header+"."+name+".allowed_values", vals);
        }

        /// Return the list of children values in a given root key.
        /** Will throw an exception if the requested root key does not exist.
        **/
        std::vector<std::string> list_values(const std::string& name = "") const {
            // Filter out __meta entry
            std::vector<std::string> ret = state::list_values(name);
            if (name == "") {
                ctl::erase_if(ret, [](const std::string& v) { return v == meta_header; });
            }

            return ret;
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
}

#endif
