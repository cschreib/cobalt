#ifndef SIGNAL_HPP
#define SIGNAL_HPP

#include <vector>
#include <memory>
#include "delegate.hpp"
#include "scoped.hpp"
#include "std_addon.hpp"
#include "variadic.hpp"

class signal_connection_t;

namespace slot {
    namespace call_adaptor {
        using none = std::nullptr_t;
    }
}

class signal_connection_t {
    delegate<void(signal_connection_t&)> stop_;

public :
    delegate<void(signal_connection_t&)> on_stop;

    template<typename F>
    signal_connection_t(F&& f) : stop_(std::forward<F>(f)) {}

    virtual ~signal_connection_t() {
        if (on_stop) {
            on_stop(*this);
        }
    }

    virtual void stop() {
        if (on_stop) {
            on_stop(*this);
            on_stop = nullptr;
        }

        stop_(*this);
    }
};

namespace signal_impl {
    template<typename T, typename ... Args>
    struct make_adaptor {
        using delegate_t = const delegate<void(Args...)>&;
        using return_t = delegate<void(signal_connection_t&, delegate_t, Args...)>;
        static return_t make() {
            return return_t(T{},
                std::integral_constant<
                    void (T::*)(signal_connection_t&, delegate_t, Args...),
                    &T::operator()
                >{}
            );
        }
    };

    template<typename ... Args>
    struct make_adaptor<std::nullptr_t, Args...> {
        using delegate_t = const delegate<void(Args...)>&;
        static delegate<void(signal_connection_t&, delegate_t, Args...)> make() {
            return nullptr;
        }
    };
}

template<typename ... Args>
class signal_t {
    using delegate_t = delegate<void(Args...)>;
    using call_adaptor_t = delegate<void(signal_connection_t&, const delegate_t&, Args...)>;

    struct slot_t {
        template<typename D, typename CA>
        slot_t(D&& d, type_list<CA>, std::unique_ptr<signal_connection_t> s) :
            callback(std::forward<D>(d)),
            call_adaptor(signal_impl::make_adaptor<CA, Args...>::make()),
            connection(std::move(s)) {}

        delegate_t callback;
        call_adaptor_t call_adaptor;
        std::unique_ptr<signal_connection_t> connection;
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

public :
    /// Create a new signal-slot connection.
    /** A connection object is returned to manage this connection. If not used, the connection will
        last "forever", i.e. as long as the signal object is alive. It means that, if the slot
        becomes dangling at some point, there is no way to stop it. To prevent this issue, one must
        carefuly handle the lifetime of the connection using the returned connection object.
        Several helpers are provided to this end (scoped_connection_t, scoped_connection_pool_t).
    **/
    template<typename CA = slot::call_adaptor::none, typename CO = signal_connection_t, typename F>
    CO& connect(F&& f) {
        static_assert(std::is_base_of<signal_connection_t,CO>::value,
            "connection type must inherit from signal_connection_t");

        slots_.emplace_back(
            std::forward<F>(f), type_list<CA>{},
            std::unique_ptr<signal_connection_t>(new CO([this](signal_connection_t& c) {
                stop_(c);
            }))
        );

        return static_cast<CO&>(*slots_.back().connection);
    }

    void dispatch(Args ... args) {
        {
            dispatching_ = true;
            auto sd = make_scoped([this]() { dispatching_ = false; });

            for (auto& s : slots_) {
                if (s.stopped) continue;
                if (s.call_adaptor) {
                    s.call_adaptor(*s.connection, s.callback, args...);
                } else {
                    s.callback(args...);
                }
            }
        }

        erase_if(slots_, [](const slot_t& s) { return s.stopped; });
    }

    void clear() {
        slots_.clear();
    }

    bool empty() const {
        return slots_.empty();
    }
};

namespace slot {
    namespace call_adaptor {
        struct once {
            template<typename ... Args>
            void operator() (signal_connection_t& c, const delegate<void(Args...)>& d, Args... args) {
                d(args...);
                c.stop();
            }
        };
    }
}

#endif
