#ifndef SIGNAL_HPP
#define SIGNAL_HPP

#include <vector>
#include <memory>
#include "delegate.hpp"
#include "scoped.hpp"
#include "ptr_vector.hpp"
#include "std_addon.hpp"
#include "variadic.hpp"

/// Class representing the connection between a signal and a slot.
/** It is owned by signal_t, and shared by reference to the initiator of the connection.
    Beside destroying the signal object, calling stop() is the only way to stop a connection and
    unregister a slot.
    Any class holding a signal_connection_base reference should fill the on_stop callback so that it
    is informed whenever the connection is stopped for any reason.
**/
class signal_connection_base {
    bool blocked_ = false;
    bool stopped_ = false; // manipulated by the signal

protected :
    // Called by the signal
    void flag_stopped_() {
        stopped_ = true;
    }

    virtual ~signal_connection_base() {
        if (on_stop) {
            on_stop(*this);
        }
    }

    virtual void signal_stop_() = 0;

    struct deleter {
        void operator() (signal_connection_base* s) const {
            delete s;
        }
    };

public :
    /// Callback function called whenever the connection is stopped and soon to be destroyed.
    ctl::delegate<void(signal_connection_base&)> on_stop;

    signal_connection_base() = default;
    signal_connection_base(const signal_connection_base&) = delete;
    signal_connection_base(signal_connection_base&&) = delete;
    signal_connection_base& operator= (const signal_connection_base&) = delete;
    signal_connection_base& operator= (signal_connection_base&&) = delete;

    /// Stop the connection.
    /** Calls all callbacks. This instance must be considered as destroyed as soon as the function
        returns.
    **/
    virtual void stop() {
        if (on_stop) {
            on_stop(*this);
            on_stop = nullptr;
        }

        signal_stop_();
    }

    /// Check wether this connection has been stopped and is about to be destroyed.
    bool stopped() const {
        return stopped_;
    }

    /// Temporarily prevent the slot from being called.
    /** Call unblock() to make it active again.
    **/
    virtual void block() {
        blocked_ = true;
    }

    /// Check if the slot is blocked. See block() and unblock().
    bool blocked() const {
        return blocked_;
    }

    /// Re-enable the slot if disabled by block().
    virtual void unblock() {
        blocked_ = false;
    }
};

/// Simplest implementation of a signal/slot connection.
/** The behavior of this connection is just to prevent the slot from being called if the connection
    is blocked.
**/
template<typename T>
class basic_signal_connection : public signal_connection_base {
    using base = signal_connection_base;
    using delegate_type = typename T::delegate_type;
    using tuple_type = typename T::tuple_type;
    using arg_type = typename T::arg_type;

    T& signal_;

protected :
    using typename base::deleter;
    friend T;

    delegate_type callback;

    template<typename F>
    basic_signal_connection(T& signal, F&& f) :
        base(), signal_(signal), callback(std::forward<F>(f)) {}

    void signal_stop_() {
        signal_.stop_(*this);
    }

    template<typename A>
    void do_call_(A&& arg) {
        ctl::tuple_to_args(callback, std::forward<A>(arg));
    }

    virtual void call_(arg_type arg) {
        if (!this->blocked()) {
            do_call_(std::forward<arg_type>(arg));
        }
    }
};

/// Same as basic_signal_connection, but will only call the slot once, then stop.
template<typename T>
class unique_signal_connection : public basic_signal_connection<T> {
    using base = basic_signal_connection<T>;
    using arg_type = typename T::arg_type;

protected :
    using typename base::deleter;
    friend T;

    template<typename F>
    unique_signal_connection(T& signal, F&& f) : base(signal, std::forward<F>(f)) {}

    void call_(arg_type arg) override {
        if (!this->blocked()) {
            this->do_call_(std::forward<arg_type>(arg));
            this->stop();
        }
    }
};

namespace signal_impl {
    template<typename T>
    using is_raw_copy_constructible = typename std::is_copy_constructible<
        typename std::decay<T>::type
    >::type;

    template<typename T>
    using is_raw_move_constructible = typename std::is_move_constructible<
        typename std::decay<T>::type
    >::type;
}

/// Helper class to handle signal/slot connections.
/** This is a generic and empty class.
    @sa template<typename ... Args, template<typename> class ConnectionBase> signal_t<void(Args...), ConnectionBase>
**/
template<typename Signature, template<typename> class ConnectionBase = basic_signal_connection>
class signal_t {
    static_assert(std::is_same<ctl::return_type<Signature>, void>::value,
        "return type of signal slot must be void");
};

/// Helper class to handle signal/slot connections.
/** A signal is a container holding a list of slots, which are basically callback functions.
    Whenever the signal is "called", it dispatches its arguments to all the slots.
    This class is tailored for use with lambda functions or functors, and does not support free
    functions.
**/
template<typename ... Args, template<typename> class ConnectionBase>
class signal_t<void(Args...), ConnectionBase> {
public :
    using signature = void(Args...);
    using delegate_type = ctl::delegate<signature>;
    using connection_type = ConnectionBase<signal_t>;
    using tuple_type = std::tuple<Args...>;
    using arg_type = tuple_type&;
    using allow_arg_move = std::false_type;
    using can_copy_construct = ctl::are_true<ctl::apply_to_type_list<
        signal_impl::is_raw_copy_constructible, ctl::function_arguments<signature>>>;
    using can_move_construct = ctl::are_true<ctl::apply_to_type_list<
        signal_impl::is_raw_move_constructible, ctl::function_arguments<signature>>>;

private :
    friend connection_type;

    using connection_ptr = std::unique_ptr<connection_type, typename connection_type::deleter>;

    bool dispatching_ = false;

    using slot_container = ctl::ptr_vector<connection_type, connection_ptr>;
    using slot_iterator = typename slot_container::iterator;
    slot_container slots_;

    slot_iterator get_connection_(connection_type& c) {
        return std::find_if(slots_.begin(), slots_.end(), [&c](const connection_type& p) {
            return &p == &c;
        });
    }

    void stop_(connection_type& c) {
        auto iter = get_connection_(c);
        if (iter == slots_.end()) return;

        if (dispatching_) {
            c.flag_stopped_();
        } else {
            slots_.erase(iter);
        }
    }

public :
    /// Create a new signal-slot connection.
    /** A connection object is returned to manage this connection. If not used, the connection will
        last "forever", i.e. as long as the signal object is alive. It means that, if the slot
        becomes dangling at some point, there is no way to stop it from being called, and thus to
        potentially crash the program. To prevent this issue, one must carefuly handle the lifetime
        of the connection using the returned connection object.
        Several helpers are provided to this end (scoped_connection_t, scoped_connection_pool_t).
        The slot is copied/moved into this signal.
    **/
    template<template<typename> class CO = ConnectionBase, typename S, typename ... TArgs>
    CO<signal_t>& connect(S&& slot, TArgs&& ... args) {
        using slot_connection = CO<signal_t>;
        static_assert(std::is_base_of<connection_type,slot_connection>::value,
            "slot connection type must inherit from signal connection type");

        connection_ptr cptr(new slot_connection(
            *this, std::forward<S>(slot), std::forward<TArgs>(args)...
        ));
        slot_connection& connection = static_cast<slot_connection&>(*cptr);

        slots_.push_back(std::move(cptr));

        return connection;
    }

    /// Trigger the signal, and dispatch it to all slots.
    template<typename ... TArgs>
    void dispatch(TArgs&& ... args) {
        static_assert(sizeof...(TArgs) == sizeof...(Args),
            "incorrect number of arguments in call");

        {
            auto sc = ctl::scoped_toggle(dispatching_);

            // Work with a copy of the slot list
            std::vector<connection_type*> local_slots;
            local_slots.reserve(slots_.size());
            for (auto& s : slots_) {
                if (s.stopped()) continue;
                local_slots.push_back(&s);
            }

            // Dispatch
            tuple_type arg(std::forward<TArgs>(args)...);
            for (auto* s : local_slots) {
                // Still need to check, they may be stopped inside a callback
                if (s->stopped()) continue;
                s->call_(arg);
            }
        }

        ctl::erase_if(slots_, [](const connection_type& s) { return s.stopped(); });
    }

    /// Stop all connections and destroy all slots.
    void clear() {
        slots_.clear();
    }

    /// Check if there is no slot registered to this signal.
    bool empty() const {
        return slots_.empty();
    }
};

/// Helper class to handle a unique signal/slot connection.
/** This is a generic and empty class.
    @sa template<typename ... Args, template<typename> class ConnectionBase> unique_signal_t<void(Args...), ConnectionBase>
**/
template<typename Signature, template<typename> class ConnectionBase = basic_signal_connection>
class unique_signal_t {
    static_assert(std::is_same<ctl::return_type<Signature>, void>::value,
        "return type of signal slot must be void");
};

/// Helper class to handle a unique signal/slot connection.
/** Contrary to signal_t, this signal type can only handle a single slot at once.
    It allows some optimizations and different behaviors. In particular, signal arguments can be
    move constructed instead of copy constructed.
**/
template<typename ... Args, template<typename> class ConnectionBase>
class unique_signal_t<void(Args...), ConnectionBase> {
public :
    using signature = void(Args...);
    using delegate_type = ctl::delegate<signature>;
    using connection_type = ConnectionBase<unique_signal_t>;
    using tuple_type = std::tuple<Args&&...>;
    using arg_type = tuple_type&&;
    using allow_arg_move = std::true_type;
    using can_copy_construct = ctl::are_true<ctl::apply_to_type_list<
        signal_impl::is_raw_copy_constructible, ctl::function_arguments<signature>>>;
    using can_move_construct = ctl::are_true<ctl::apply_to_type_list<
        signal_impl::is_raw_move_constructible, ctl::function_arguments<signature>>>;

private :
    friend connection_type;

    using connection_ptr = std::unique_ptr<connection_type, typename connection_type::deleter>;
    connection_ptr slot_;

    void stop_(connection_type& c) {
        if (slot_.get() != &c) return;
        slot_ = nullptr;
    }

public :
    /// Create a new signal-slot connection.
    /** A connection object is returned to manage this connection. If not used, the connection will
        last "forever", i.e. as long as the signal object is alive. It means that, if the slot
        becomes dangling at some point, there is no way to stop it from being called, and thus to
        potentially crash the program. To prevent this issue, one must carefuly handle the lifetime
        of the connection using the returned connection object.
        Several helpers are provided to this end (scoped_connection_t, scoped_connection_pool_t).
        The slot is copied/moved into this signal.
    **/
    template<template<typename> class CO = ConnectionBase, typename S, typename ... TArgs>
    CO<unique_signal_t>& connect(S&& slot, TArgs&& ... args) {
        using slot_connection = CO<unique_signal_t>;
        static_assert(std::is_base_of<connection_type,slot_connection>::value,
            "slot connection type must inherit from signal connection type");

        slot_ = connection_ptr(new slot_connection(
            *this, std::forward<S>(slot), std::forward<TArgs>(args)...
        ));

        return static_cast<slot_connection&>(*slot_);
    }

    /// Trigger the signal and dispatch it to the slot, if any.
    template<typename ... TArgs>
    void dispatch(TArgs&& ... args) {
        if (!slot_) return;
        tuple_type t(std::forward<TArgs>(args)...);
        slot_->call_(std::move(t));
    }

    /// Stop the connection and destroy the slot, if any.
    void clear() {
        slot_ = nullptr;
    }

    /// Check if there is no slot registered to this signal.
    bool empty() const {
        return slot_ == nullptr;
    }
};

#endif
