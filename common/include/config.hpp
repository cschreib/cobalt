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
    public :
        struct parsing_failure : public std::exception {
            explicit parsing_failure(const std::string& message) : message(message) {}
            parsing_failure(const parsing_failure&) = default;
            parsing_failure(parsing_failure&&) = default;
            virtual ~parsing_failure() noexcept {}
            const char* what() const noexcept override { return message.c_str(); }
            std::string message;
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
            current tree structure is not compatible with the provided parameter name. It will also
            throw if an exception is raised by any of the callbacks registered to this particular
            parameter; in such a case, the value of the parameter is left unchanged.
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
            the current tree structure is not compatible with the provided parameter name. It will
            also throw if an exception is raised by any of the callbacks registered to this
            particular parameter; in such a case, the value of the parameter is left unchanged.
            Compared to set_value(), this version takes directly a string for the new value of the
            parameter, which is stored untouched into the configuration tree.
        **/
        void set_raw_value(const std::string& name, std::string value) {
            config_node& node = tree_.reach(name);
            set_raw_value_(node, name, std::move(value));
        }

        /// Retrieve the value of a parameter from this configuration state.
        /** If the parameter exists, then its value is written in "value", and 'true' is returned
            if the parsing to type T was successful. Else 'false' is returned and "value" is left
            untouched.
        **/
        template<typename T>
        bool get_value(const std::string& name, T& value) const {
            const config_node* node = tree_.try_reach(name);
            if (!node || node->is_empty) {
                return false;
            }

            return string::stringify<T>::parse(value, node->value);
        }

        /// Retrieve the value of a parameter from this configuration state as a string.
        /** If the parameter exists, then its value is written in "value", and 'true' is returned.
            Else 'false' is returned and "value" is left untouched.
            Compared to get_value(), this version will write the value as a string, whatever the
            real underlying type is.
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
        /** If the parameter exists, then its value is parsed into "value", and 'true' is returned
            if the parsing to type T was successful. Else, the default value is stored in the
            configuration tree, and parsed into "value". This function will return 'false' only if
            the parsing fails, and will throw if the tree structure is incompatible with the
            provided name, or if setting the default value raises an exception in one of the
            registered callbacks
        **/
        template<typename T, typename N>
        bool get_value(const std::string& name, T& value, const N& def) {
            config_node& node = tree_.reach(name);
            if (node.is_empty) {
                std::string string_value;
                string::stringify<N>::serialize(def, string_value);
                set_raw_value_(node, name, std::move(string_value));
            }

            return string::stringify<T>::parse(value, node.value);
        }

        /// Check if a parameter exists.
        bool value_exists(const std::string& name) const {
            return tree_.try_reach(name) != nullptr;
        }

        /// Return the list of children values in a given root key.
        /** Will return an empty list if the root key does not exist.
        **/
        std::vector<std::string> list_values(const std::string& name = "") const {
            std::vector<std::string> ret;

            const auto* branch = (name == "" ? &tree_.root() : tree_.try_reach_branch(name));
            if (branch != nullptr) {
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
                    throw parsing_failure("could not parse "+name+" from value '"+value+"'");
                }
            };

            signal_connection_base& sc = node.signal.connect(parse_to_var);

            if (node.is_empty) {
                // Parameter does not yet have a value, set it from variable we just bound
                // and trigger signals
                std::string string_value;
                string::stringify<T>::serialize(var, std::move(string_value));
                set_raw_value_(node, name, string_value);
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
                    throw parsing_failure("could not parse "+name+" from value '"+value+"'");
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
                    throw parsing_failure("could not parse "+name+" from value '"+value+"'");
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
        struct config_node {
            std::string value;
            bool        is_empty = true;
            signal_t<void(const std::string&)> signal;
        };

        void save_node_(std::ostream& f, const ctl::string_tree<config_node>::branch& node,
            const std::string& name) const;

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

        ctl::string_tree<config_node> tree_;
        mutable bool dirty_;
    };
}

#endif
