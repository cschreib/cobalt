#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "string_tree.hpp"
#include "stringify.hpp"
#include <sstream>
#include <functional>

namespace config {
namespace config_impl {
    template<typename T>
    struct same_impl {
        using type = T;
    };

    template<typename T>
    using same = same_impl<T>;

    template<typename T>
    using std_function = std::function<void(const T&)>;

    template<typename T>
    struct is_function {
        template <typename U> static std::true_type  dummy(decltype(&U::operator())*);
        template <typename U> static std::false_type dummy(...);
        static const bool value = decltype(dummy<T>(0))::value;
    };

    template<typename Ret, typename ... Args>
    struct is_function<Ret(Args...)> : public std::true_type {};
    template<typename Ret, typename ... Args>
    struct is_function<Ret(*)(Args...)> : public std::true_type {};

    template<typename T>
    using remove_cr = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

    template<typename C, typename T>
    using fptr = void (C::*) (T);
    template<typename C, typename T>
    using cfptr = void (C::*) (T) const;
    template<typename C, typename T>
    using fcrptr = void (C::*) (const T&);
    template<typename C, typename T>
    using cfcrptr = void (C::*) (const T&) const;

    template<typename C, typename T>
    T get_first_param_type(const fptr<C,T> p) { return *(T*)nullptr; }
    template<typename C, typename T>
    T get_first_param_type(const cfptr<C,T> p) { return *(T*)nullptr; }
    template<typename C, typename T>
    T get_first_param_type(const fcrptr<C,T> p) { return *(T*)nullptr; }
    template<typename C, typename T>
    T get_first_param_type(const cfcrptr<C,T> p) { return *(T*)nullptr; }

    template<typename F, bool func>
    struct extract_param_impl;

    template<typename F>
    struct extract_param_impl<F, /* bool func = */ false> {
        using type = F;
    };

    template<typename F>
    struct extract_param_impl<F, /* bool func = */ true> {
        using type = decltype(get_first_param_type(&F::operator()));
    };
    template<typename T>
    struct extract_param_impl<void(*)(const T&), /* bool func = */ true> {
        using type = T;
    };
    template<typename T>
    struct extract_param_impl<void(*)(T), /* bool func = */ true> {
        using type = T;
    };

    template<typename T>
    using extract_param = typename extract_param_impl<T, is_function<T>::value>::type;

    template<typename T>
    using is_config_function = is_function<T>;

    template<typename T>
    struct make_std_function_impl {
        template<typename F>
        static std_function<T> make(F&& f) { return std_function<T>(std::move(f)); }
    };

    template<typename T>
    struct make_std_function_impl<std_function<T>> {
        static std_function<T> make(std_function<T>&& f) { return std::move(f); }
    };

    template<typename T, typename F>
    std_function<T> make_std_function(F&& f) {
        return make_std_function_impl<T>::make(std::move(f));
    }
}

    /// Holds all the configurable parameters of the game.
    /** It is advised to use the config::helper instead of this
        class directly, since it automatically takes care of
        unbinding configurable values and config functions.

        The configurable parameters are arranged in a tree,
        for clarity and (hopefully) faster traversal. For
        example :
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

        In this tree, "graphics", "window" and "ui" are
        branches, in the sense that they do not contain any
        value by themselves, but lead to other parameters.
        All the other items are called "leaves" : they hold
        a single parameter value.

        Also note that, for a new parameter to be created,
        its associated C++ type T must be "configurable", that
        is there must exist a specialization of the configure
        template class for T. This specialization must provide
        two static functions :
            bool parse(T& t, const std::string& value);
            void serialize(const T& t, std::string& value);
        "parse" tries to read out a value from "value" (it must
        return false if it fails to do so) and store it in t,
        and "parse" does the opposite. There is a default
        implementation that uses string conversion via standard
        string streams, so if a type provides both the >> and
        << iostream operators, it is "configurable".
    **/
    class state {
    public :
        friend struct helper;

        /// Default constructor.
        /** Constructs an empty configuration state, no data is loaded.
        */
        state();

        /// Type of the ID associated to each binded object.
        using id_t = std::ptrdiff_t;

        /// Optional output flux for printing errors.
        mutable std::ostream out;

        /// Read a plain text configuration file and load its values.
        /** The configuration file is a succession of lines, each
        *   containing a single configuration value. A line is
        *   made of :
        *   1) the name of the parameter (ex: "graphics.resolution")
        *   2) the value of the parameter enclosed in parenthesis
        *      (ex: "(800, 600)")
        */
        void parse(const std::string& file);

        /// Save the current configuration in a plain text file.
        /** If any, all the content of this file is erased before
        *   the new configuration is written.
        *   For more information on the file format, see parse().
        */
        void save(const std::string& file) const;

        /// Remove all configuration items and bonds.
        void clear();

        /// Set the value of a parameter.
        /** If the parameter already exists, then its value is
        *   updated and all binded objects are notified of the
        *   change (only if the value is different than the
        *   previous one).
        *   Else, the parameter is created.
        *   This function will fail if the current tree structure
        *   is not compatible with the provided parameter name.
        *   \param t    The value to store
        *   \param name Where to store this value
        */
        template<typename T>
        bool set_value(const T& t, const std::string& name) {
            try {
                config_node& node = tree_.reach(name);
                if (!node.is_empty) {
                    std::string old_value = node.value;
                    stringify<T>::serialize(t, node.value);
                    if (node.value != old_value) {
                        for (auto& b : node.binds) {
                            b->set((void*)&t);
                        }
                        dirty_ = true;
                    }
                } else {
                    stringify<T>::serialize(t, node.value);
                    node.is_empty = false;
                    dirty_ = true;
                }
                return true;
            } catch (string_tree<config_node>::wrong_structure_exception& e) {
                out << "error: " << e.what() << std::endl;
                return false;
            }
        }

        /// Retrieve a the value from this configuration state.
        /** If the parameter exists, then its value is written in
        *   the variable t, and 'true' is returned.
        *   Else 'false' is returned.
        *   \param[out] t    Where the value is written
        *   \param      name Where to look for the value
        */
        template<typename T>
        bool get_value(T& t, const std::string& name) const {
            const config_node* node = tree_.try_reach(name);
            if (node) {
                if (node->is_empty) {
                    return false;
                } else {
                    return stringify<T>::parse(t, node->value);
                }
            } else {
                return false;
            }
        }

        /// Retrieve a the value from this configuration state with a default value.
        /** If the parameter exists, then its value is written in
        *   the variable t, and 'true' is returned. Else, the
        *   default value is written in t and in the configuration
        *   and 'true' is also returned. 'false' will only be
        *   returned if the configuration tree is incompatible
        *   with 'name'.
        *   \param[out] t    Where the value is written
        *   \param      name Where to look for the value
        *   \param      def  The default value
        */
        template<typename T, typename N>
        bool get_value(T& t, const std::string& name, const N& def) {
            try {
                config_node& node = tree_.reach(name);
                if (node.is_empty) {
                    t = def;
                    stringify<T>::serialize(T(def), node.value);
                    node.is_empty = false;
                    dirty_ = true;
                    return true;
                } else {
                    return stringify<T>::parse(t, node.value);
                }
                return true;
            } catch (string_tree<config_node>::wrong_structure_exception& e) {
                out << "error: " << e.what() << std::endl;
                return false;
            }
        }

        /// Check if a parameter exists.
        bool value_exists(const std::string& name) const {
            return tree_.try_reach(name) != nullptr;
        }

        /// Bind a variable to a configurable parameter.
        /** Using this method, one can bind a C++ variable to a
        *   parameter in this configuration state. If the binding is
        *   successful (i.e. if the provided parameter name is
        *   valid), then a unique identification number is stored
        *   in the variable id. This number can be used later on
        *   to unbind this variable.
        *   When the value of the parameter changes, this variable
        *   is automatically updated.
        *   This function will fail if the current tree structure
        *   is not compatible with the provided parameter name.
        *   \param t    The variable to bind
        *   \param id   The unique ID identifying the bond
        *   \param name The name of the parameter to bind
        */
        template<typename T>
        bool bind(T& t, id_t& id, const std::string& name) {
            try {
                config_node& node = tree_.reach(name);
                node.binds.push_back(std::unique_ptr<any_conf>(new conf_variable<T>(t)));
                any_conf& b = *node.binds.back();
                id = b.key();
                if (node.is_empty) {
                    stringify<T>::serialize(t, node.value);
                    node.is_empty = false;
                    dirty_ = true;
                } else {
                    b.read(node.value);
                }
                return true;
            } catch (string_tree<config_node>::wrong_structure_exception& e) {
                out << "error: " << e.what() << std::endl;
                return false;
            }
        }

        /// Bind a callback function to a configurable parameter.
        /** This method can be used to register a callback function
        *   that will be executed each time the binded parameter is
        *   modified. This callback function can be any kind of
        *   function that can be bound to an std::function, that is
        *   either a C++ function, a functor class, a C++ lambda,
        *   or another std::function. If the binding is
        *   successful (i.e. if the provided parameter name is
        *   valid), then a unique identification number is stored
        *   in the variable id. This number can be used later on
        *   to unbind this callback.
        *   This function will fail if the current tree structure
        *   is not compatible with the provided parameter name.
        *   \param f    The callback to bind
        *   \param id   The unique ID identifying the bond
        *   \param name The name of the parameter to bind
        */
        template<typename F, typename enable =
            typename std::enable_if<config_impl::is_config_function<F>::value>::type>
        bool bind(F&& f, id_t& id, const std::string& name) {
            using param_t = config_impl::extract_param<F>;
            try {
                config_node& node = tree_.reach(name);
                node.binds.push_back(std::unique_ptr<any_conf>(new conf_function<param_t>(
                    config_impl::make_std_function<param_t>(std::move(f))
                )));
                any_conf& b = *node.binds.back();
                id = b.key();
                if (!node.is_empty) {
                    b.read(node.value);
                }
                return true;
            } catch (string_tree<config_node>::wrong_structure_exception& e) {
                out << "error: " << e.what() << std::endl;
                return false;
            }
        }

        /// Bind a callback function to a configurable parameter with a default value.
        /** See the other bind() method for a detailed description.
        *   This overloaded version also takes a default value that
        *   will be saved then fed to the callback function if the
        *   parameter does not yet exist.
        *   \param f    The callback to bind
        *   \param id   The unique ID identifying the bond
        *   \param name The name of the parameter to bind
        *   \param def  The default value
        */
        template<typename F, typename enable =
            typename std::enable_if<config_impl::is_config_function<F>::value>::type>
        bool bind(F&& f, id_t& id, const std::string& name,
            const config_impl::extract_param<F>& def) {

            using param_t = config_impl::extract_param<F>;
            try {
                config_node& node = tree_.reach(name);
                node.binds.push_back(std::unique_ptr<any_conf>(new conf_function<param_t>(
                    config_impl::make_std_function<param_t>(std::move(f))
                )));
                any_conf& b = *node.binds.back();
                id = b.key();
                if (node.is_empty) {
                    stringify<param_t>::serialize(def, node.value);
                    b.set((void*)&def);
                    node.is_empty = false;
                    dirty_ = true;
                } else {
                    b.read(node.value);
                }
                return true;
            } catch (string_tree<config_node>::wrong_structure_exception& e) {
                out << "error: " << e.what() << std::endl;
                return false;
            }
        }

        /// Unbind a variable or a callback function from a configurable parameter.
        /** If the bound variable is destroyed or if the callback
        *   function should be disabled, then it is necessary to
        *   unbind them. This can be done using this method, by
        *   providing the unique id that was given to the bond
        *   when bind() was called.
        *   This function will fail if the current tree structure
        *   is not compatible with the provided parameter name.
        */
        bool unbind(id_t id, const std::string& name) {
            try {
                config_node& node = tree_.reach(name);
                for (auto iter = node.binds.begin(); iter != node.binds.end(); ++iter) {
                    if ((*iter)->key() == id) {
                        node.binds.erase(iter);
                        return true;
                    }
                }
                return false;
            } catch (string_tree<config_node>::wrong_structure_exception& e) {
                out << "error: " << e.what() << std::endl;
                return false;
            }
        }

    private :
        struct any_conf {
            virtual ~any_conf() {}
            virtual void read(const std::string& value) = 0;
            virtual void set(void* data) = 0;
            virtual id_t key() const = 0;
        };

        template<typename T>
        struct conf_variable : public any_conf {
            conf_variable(T& t) : t(t) {}

            T& t;

            void read(const std::string& value) override {
                stringify<T>::parse(t, value);
            }
            void set(void* data) override {
                t = *(T*)data;
            }
            id_t key() const override {
                return (id_t)&t;
            }
        };

        template<typename T>
        struct conf_function : public any_conf {
            conf_function(std::function<void(const T&)>&& f) : f(std::move(f)) {}

            std::function<void(const T&)> f;

            void read(const std::string& value) override {
                T t;
                if (stringify<T>::parse(t, value)) {
                    f(t);
                }
            }
            void set(void* data) override {
                f(*(T*)data);
            }
            id_t key() const override {
                return (id_t)&f;
            }
        };

        struct config_node {
            std::string value;
            bool        is_empty = true;
            std::vector<std::unique_ptr<any_conf>> binds;
        };

        void save_node_(std::ofstream& f, const string_tree<config_node>::branch& node,
            const std::string& name) const;

        string_tree<config_node> tree_;
        mutable bool dirty_;
    };

    /// Helper to simplify the usage of config.
    /** This class is a simple wrapper around the config
    *   class, that automatically takes care of unbinding
    *   the variables and callback functions that one has
    *   bound via its interface.
    *   All bonds are broken when this helper is destroyed.
    **/
    struct helper {
        /// Default constructor.
        /** It is an error to use any of the following member
        *   functions on an default constructed config::helper.
        */
        helper() : conf(nullptr) {}

        /// Wrap an existing config.
        explicit helper(state& conf) : conf(&conf) {}

        /// Wrap an existing config with a hint of how many bond it will create.
        /** The hint is used to pre-allocate some memory if
        *   a great number of configuration items are to be
        *   registered.
        */
        helper(state& conf, std::size_t n) : conf(&conf) {
            items.reserve(n);
        }

        /// Destructor.
        /** Unbinds all the previously bound variables and
        *   callback functions.
        */
        ~helper() {
            for (auto& i : items) {
                conf->unbind(i.id, i.name);
            }
            items.clear();
        }

        /// Set the value of a parameter.
        /** If the parameter already exists, then its value is
        *   updated and all binded objects are notified of the
        *   change (only if the value is different than the
        *   previous one).
        *   Else, the parameter is created.
        *   This function will fail if the current tree structure
        *   is not compatible with the provided parameter name.
        *   \param t    The value to store
        *   \param name Where to store this value
        */
        template<typename T>
        bool set_value(const T& t, const std::string& name) {
            return conf->set_value(t, name);
        }

        /// Retrieve a the value from this configuration state.
        /** If the parameter exists, then its value is written in
        *   the variable t, and 'true' is returned.
        *   Else 'false' is returned.
        *   \param[out] t    Where the value is written
        *   \param      name Where to look for the value
        */
        template<typename T>
        bool get_value(T& t, const std::string& name) const {
            return conf->get_value(t, name);
        }

        /// Retrieve a the value from this configuration state with a default value.
        /** If the parameter exists, then its value is written in
        *   the variable t, and 'true' is returned. Else, the
        *   default value is written in t and in the configuration
        *   and 'true' is also returned. 'false' will only be
        *   returned if the configuration tree is incompatible
        *   with 'name'.
        *   \param[out] t    Where the value is written
        *   \param      name Where to look for the value
        *   \param      def  The default value
        */
        template<typename T, typename N>
        bool get_value(T& t, const std::string& name, const N& def) {
            return conf->get_value(t, name, def);
        }

        /// Check if a parameter exists.
        bool value_exists(const std::string& name) const {
            return conf->value_exists(name);
        }

        /// Bind a variable to a configurable parameter.
        /** Using this method, one can bind a C++ variable to a
        *   parameter in this configuration state.
        *   When the value of the parameter changes, this variable
        *   is automatically updated.
        *   This function will fail if the current tree structure
        *   is not compatible with the provided parameter name.
        *   \param t    The variable to bind
        *   \param name The name of the parameter to bind
        */
        template<typename T>
        bool bind(T& t, const std::string& name) {
            item i(name);
            if (conf->bind(t, i.id, name)) {
                items.push_back(i);
                return true;
            }
            return false;
        }

        /// Bind a callback function to a configurable parameter.
        /** This method can be used to register a callback function
        *   that will be executed each time the binded parameter is
        *   modified. This callback function can be any kind of
        *   function that can be bound to an std::function, that is
        *   either a C++ function, a functor class, a C++ lambda,
        *   or another std::function.
        *   This function will fail if the current tree structure
        *   is not compatible with the provided parameter name.
        *   \param f    The callback to bind
        *   \param name The name of the parameter to bind
        */
        template<typename F, typename enable =
            typename std::enable_if<config_impl::is_config_function<F>::value>::type>
        bool bind(F&& f, const std::string& name) {
            item i(name);
            if (conf->bind(std::move(f), i.id, name)) {
                items.push_back(i);
                return true;
            }
            return false;
        }

        /// Bind a callback function to a configurable parameter with a default value.
        /** This method can be used to register a callback function
        *   that will be executed each time the binded parameter is
        *   modified. This callback function can be any kind of
        *   function that can be bound to an std::function, that is
        *   either a C++ function, a functor class, a C++ lambda,
        *   or another std::function.
        *   This overloaded version also takes a default value that
        *   will be saved then fed to the callback function if the
        *   parameter does not yet exist.
        *   This function will fail if the current tree structure
        *   is not compatible with the provided parameter name.
        *   \param f    The callback to bind
        *   \param name The name of the parameter to bind
        *   \param def  The default value
        */
        template<typename F, typename enable =
            typename std::enable_if<config_impl::is_config_function<F>::value>::type>
        bool bind(F&&f, const std::string& name, const config_impl::extract_param<F>& def) {
            item i(name);
            if (conf->bind(std::move(f), i.id, name, def)) {
                items.push_back(i);
                return true;
            }
            return false;
        }

    private :
        struct item {
            item(const std::string& name) : id(0), name(name) {}
            state::id_t id;
            std::string name;
        };

        state*            conf;
        std::vector<item> items;
    };
}

#endif
