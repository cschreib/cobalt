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
            ctl::has_right_shift<std::ostream, T>::value
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
        /// Default constructor.
        /** Constructs an empty configuration state, no data is loaded.
        **/
        state();

        state(const state&) = delete;
        state(state&&) = default;
        state& operator= (const state&) = delete;
        state& operator= (state&&) = default;

        /// Read a plain text configuration file and load its values.
        /** The configuration file is a succession of lines, each containing a single configuration
            value. A line is made of:
             - the name of the parameter (ex: "graphics.resolution")
             - the value of the parameter enclosed in parenthesis (ex: "(800, 600)")
        **/
        void parse(const std::string& file);

        /// Save the current configuration in a plain text file.
        /** If any, all the content of this file is erased before the new configuration is written.
            For more information on the file format, see parse().
        **/
        void save(const std::string& file) const;

        /// Remove all configuration items and bonds.
        void clear();

        /// Set the value of a parameter.
        /** If the parameter already exists, then its value is updated and all bound objects are
            notified of the change (only if the value is different than the previous one). Else, the
            parameter is created. This function will throw if the current tree structure is not
            compatible with the provided parameter name.
        **/
        template<typename T>
        void set_value(const std::string& name, const T& value) {
            config_node& node = tree_.reach(name);
            if (!node.is_empty) {
                std::string old_value = node.value;
                string::stringify<T>::serialize(value, node.value);
                if (node.value != old_value) {
                    node.signal.dispatch(node.value);
                    dirty_ = true;
                }
            } else {
                string::stringify<T>::serialize(value, node.value);
                node.is_empty = false;
                dirty_ = true;
            }
        }

        /// Retrieve a the value from this configuration state.
        /** If the parameter exists, then its value is written in "value", and 'true' is returned.
            Else 'false' is returned.
        **/
        template<typename T>
        bool get_value(const std::string& name, T& value) const {
            const config_node* node = tree_.try_reach(name);
            if (!node || node->is_empty) {
                return false;
            }

            return string::stringify<T>::parse(value, node->value);
        }

        /// Retrieve a the value from this configuration state with a default value.
        /** If the parameter exists, then its value is written in "value", and 'true' is returned.
            Else, the default value is stored in the configuration tree, and deserialized into
            "value". This function will return 'false' only if the deserialization fails, and will
            throw if the tree structure is incompatible with the provided name.
        **/
        template<typename T, typename N>
        bool get_value(const std::string& name, T& value, const N& def) {
            config_node& node = tree_.reach(name);
            if (node.is_empty) {
                string::stringify<N>::serialize(def, node.value);
                node.is_empty = false;
                dirty_ = true;
            }

            return string::stringify<T>::parse(value, node.value);
        }

        /// Check if a parameter exists.
        bool value_exists(const std::string& name) const {
            return tree_.try_reach(name) != nullptr;
        }

        /// Bind a variable to a configurable parameter.
        /** Using this method, one can bind a C++ variable to a parameter in this configuration
            state. When the value of the parameter changes, this variable is automatically updated.
            This function will throw if the current tree structure is not compatible with the
            provided parameter name.
        **/
        template<typename T>
        signal_connection_base& bind(const std::string& name, T& var) {
            static_assert(impl::is_configurable<T>::value,
                "bound variable must be configurable");

            config_node& node = tree_.reach(name);
            signal_connection_base& sc = node.signal.connect([&var](const std::string& value) {
                string::stringify<T>::parse(var, value);
            });

            if (node.is_empty) {
                string::stringify<T>::serialize(var, node.value);
                node.is_empty = false;
                dirty_ = true;
            } else {
                string::stringify<T>::parse(var, node.value);
            }

            return sc;
        }

        /// Bind a callback function to a configurable parameter.
        /** This method can be used to register a callback function that will be executed each time
            the bound parameter is modified. This callback function is a functor, and can only take
            a single configurable argument. This function will throw if the current tree structure
            is not compatible with the provided parameter name.
        **/
        template<typename F, typename enable =
            typename std::enable_if<!impl::is_configurable<F>::value>::type>
        signal_connection_base& bind(const std::string& name, F&& func) {
            static_assert(ctl::argument_count<F>::value == 1,
                "configuration callback can only take one argument");
            using ArgType = typename std::decay<ctl::functor_argument<F>>::type;
            static_assert(impl::is_configurable<ArgType>::value,
                "configuration callback argument must be configurable");

            config_node& node = tree_.reach(name);
            signal_connection_base& sc = node.signal.connect([func](const std::string& value) {
                ArgType t;
                if (string::stringify<ArgType>::parse(t, value)) {
                    func(t);
                }
            });

            if (!node.is_empty) {
                ArgType t;
                if (string::stringify<ArgType>::parse(t, node.value)) {
                    func(t);
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
            using ArgType = typename std::decay<ctl::functor_argument<F>>::type;
            static_assert(impl::is_configurable<ArgType>::value,
                "configuration callback argument must be configurable");

            config_node& node = tree_.reach(name);
            signal_connection_base& sc = node.signal.connect([func](const std::string& value) {
                ArgType t;
                if (string::stringify<ArgType>::parse(t, value)) {
                    func(t);
                }
            });

            if (node.is_empty) {
                string::stringify<N>::serialize(def, node.value);
                node.signal.dispatch(node.value);
                node.is_empty = false;
                dirty_ = true;
            } else {
                ArgType t;
                if (string::stringify<ArgType>::parse(t, node.value)) {
                    func(t);
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

        void save_node_(std::ofstream& f, const ctl::string_tree<config_node>::branch& node,
            const std::string& name) const;

        ctl::string_tree<config_node> tree_;
        mutable bool dirty_;
    };
}

#endif
