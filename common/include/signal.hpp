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
    Any class holding a signal_connection_t reference should fill the on_stop callback so that it is
    informed whenever the connection is stopped for any reason.
**/
class signal_connection_t {
    ctl::delegate<void(signal_connection_t&)> stop_;

protected :
    template<typename ... Args>
    friend class signal_t;

    template<typename F>
    signal_connection_t(F&& f) : stop_(std::forward<F>(f)) {}

    virtual ~signal_connection_t() {
        if (on_stop) {
            on_stop(*this);
        }
    }

    struct deleter {
        void operator() (signal_connection_t* s) const {
            delete s;
        }
    };

public :
    /// Callback function called whenever the connection is stopped and soon to be destroyed.
    ctl::delegate<void(signal_connection_t&)> on_stop;

    signal_connection_t(const signal_connection_t&) = delete;
    signal_connection_t(signal_connection_t&&) = delete;
    signal_connection_t& operator= (const signal_connection_t&) = delete;
    signal_connection_t& operator= (signal_connection_t&&) = delete;

    /// Stops the connection.
    /** Calls all callbacks. This instance must be considered as destroyed as soon as the function
        returns.
    **/
    virtual void stop() {
        if (on_stop) {
            on_stop(*this);
            on_stop = nullptr;
        }

        stop_(*this);
    }
};

namespace signal_impl {
    template<typename T>
    using first_argument = ctl::type_list_element<0, ctl::function_arguments<T>>;

    template<typename CO, typename T>
    using is_extended_callback__ = std::integral_constant<bool,
        std::is_convertible<CO&, first_argument<T>>::value
    >;

    template<bool V, typename CO, typename T>
    struct is_extended_callback_ : std::false_type {};

    template<typename CO, typename T>
    struct is_extended_callback_<true, CO, T> : is_extended_callback__<CO,T> {};

    template<typename CO, typename T>
    using is_extended_callback = typename is_extended_callback_<
        (ctl::argument_count<T>::value > 0), CO, T
    >::type;
}

/// Helper class to handle signal/slot connections.
/** A signal is a container holding a list of slots, which are basically callback functions.
    Whenever the signal is "called", it dispatches its arguments to all the slots.
    This class is tailored for use with lambda functions or functors, and does not support free
    functions.
**/
template<typename ... Args>
class signal_t {
    using delegate_t = ctl::delegate<void(Args...)>;
    using call_adaptor_t = ctl::delegate<void(signal_connection_t&, const delegate_t&, Args...)>;

    struct slot_t {
        using connection_t = std::unique_ptr<signal_connection_t, signal_connection_t::deleter>;

        template<typename D>
        slot_t(D&& d, connection_t s) :
            callback(std::forward<D>(d)),
            connection(std::move(s)) {}

        delegate_t callback;
        call_adaptor_t call_adaptor;
        connection_t connection;
        bool stopped = false;
    };

    bool dispatching_ = false;
    std::vector<slot_t> slots_;

    void stop_(signal_connection_t& c) {
        for (auto iter = slots_.begin(); iter != slots_.end(); ++iter) {
            if (iter->connection.get() == &c) {
                if (dispatching_) {
                    iter->stopped = true;
                } else {
                    slots_.erase(iter);
                }
                break;
            }
        }
    }

    template<typename CO, typename F>
    CO& connect_(F&& f, std::false_type) {
        slots_.emplace_back(
            std::forward<F>(f),
            typename slot_t::connection_t(new CO([this](signal_connection_t& c) {
                stop_(c);
            }))
        );

        return static_cast<CO&>(*slots_.back().connection);
    }

    template<typename CO, typename F>
    CO& connect_(F&& f, std::true_type) {
        typename slot_t::connection_t cp(new CO([this](signal_connection_t& c) {
            stop_(c);
        }));

        CO& c = static_cast<CO&>(*cp);

        slots_.emplace_back(
            [f,&c](Args... args) mutable {
                f(c, args...);
            },
            std::move(cp)
        );

        return c;
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
    template<typename CO = signal_connection_t, typename F>
    CO& connect(F&& f) {
        static_assert(std::is_base_of<signal_connection_t,CO>::value,
            "connection type must inherit from signal_connection_t");

        using FType = typename std::decay<F>::type;
        return connect_<CO>(std::forward<F>(f),
            signal_impl::is_extended_callback<CO, decltype(&FType::operator())>()
        );
    }

    /// Trigger the signal, and dispatch it to all slots.
    void dispatch(Args ... args) {
        {
            dispatching_ = true;
            auto sd = ctl::make_scoped([this]() { dispatching_ = false; });

            for (auto& s : slots_) {
                if (s.stopped) continue;
                s.callback(args...);
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
