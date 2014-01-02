#ifndef SIGNAL_HPP
#define SIGNAL_HPP

#include <vector>
#include <memory>
#include "delegate.hpp"
#include "scoped.hpp"
#include "std_addon.hpp"
#include "variadic.hpp"

/// Class representing the connection between a signal and a slot.
/** It is owned by signal_t, and shared by reference to the initiator of the connection.
    Beside destroying the signal object, calling stop() it is the only way to stop a connection and
    unregister a slot.
    Any class holding a signal_connection_base reference should fill the on_stop callback so that it
    is informed whenever the connection is stopped for any reason.
**/
class signal_connection_base {
    bool blocked_ = false;

protected :
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

    virtual void call_(tuple_type& arg) {
        if (!this->blocked()) {
            ctl::tuple_to_args(callback, arg);
        }
    }
};

/// Same as basic_signal_connection, but will only call the slot once, then stop.
template<typename T>
class unique_signal_connection : public basic_signal_connection<T> {
    using base = basic_signal_connection<T>;
    using delegate_type = typename T::delegate_type;
    using tuple_type = typename T::tuple_type;

protected :
    using typename base::deleter;
    friend T;

    template<typename F>
    unique_signal_connection(T& signal, F&& f) : base(signal, std::forward<F>(f)) {}

    virtual void call_(tuple_type& arg) {
        if (!this->blocked()) {
            ctl::tuple_to_args(this->callback, arg);
            this->stop();
        }
    }
};

/// \cond DOXYGEN_EXCLUDE
template<typename Signature, template<typename> class ConnectionBase = basic_signal_connection>
class signal_t {
    static_assert(std::is_same<typename ctl::return_type<Signature>::type, void>::value,
        "return type of signal slot must be void");
};
/// \endcond

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

private :
    friend connection_type;

    using connection_ptr = std::unique_ptr<connection_type, typename connection_type::deleter>;

    struct slot_t {
        slot_t(connection_ptr s) : connection(std::move(s)) {}
        slot_t(slot_t&&) = default;
        slot_t& operator= (slot_t&&) = default;

        connection_ptr connection;
        bool stopped = false;
    };

    bool dispatching_ = false;

    using slot_container = std::vector<slot_t>;
    using slot_iterator = typename slot_container::iterator;
    slot_container slots_;

    slot_iterator get_connection_(connection_type& c) {
        for (auto iter = slots_.begin(); iter != slots_.end(); ++iter) {
            if (iter->connection.get() == &c) {
                return iter;
            }
        }

        return slots_.end();
    }

    void stop_(connection_type& c) {
        auto iter = get_connection_(c);
        if (iter == slots_.end()) return;

        if (dispatching_) {
            iter->stopped = true;
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

        slots_.emplace_back(std::move(cptr));

        return connection;
    }

    /// Trigger the signal, and dispatch it to all slots.
    template<typename ... TArgs>
    void dispatch(TArgs&& ... args) {
        {
            dispatching_ = true;
            auto sd = ctl::make_scoped([this]() { dispatching_ = false; });

            tuple_type arg(std::forward<TArgs>(args)...);
            for (auto& s : slots_) {
                if (s.stopped) continue;
                s.connection->call_(arg);
            }
        }

        ctl::erase_if(slots_, [](const slot_t& s) { return s.stopped; });
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

#endif
