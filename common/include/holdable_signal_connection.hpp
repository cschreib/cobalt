#ifndef HOLDABLE_SIGNAL_CONNECTION_HPP
#define HOLDABLE_SIGNAL_CONNECTION_HPP

#include "signal.hpp"

/// Signal/slot connection that holds signal arguments when blocked and releases them when unblocked.
/** The difference with basic_signal_connection is that, when the connection is blocked, any call to
    the slot is still blocked, but the arguments are stored inside a local container. When the
    connection is unblocked, all the arguments that were held back are released, and forwarded to
    the slot, as if they just arrived "late".
**/
template<typename T>
class holdable_signal_connection : public basic_signal_connection<T> {
protected :
    using base = basic_signal_connection<T>;
    using delegate_type = typename T::delegate_type;
    using tuple_type = typename T::tuple_type;

    std::vector<tuple_type> args_;

    using typename base::deleter;
    friend T;

    template<typename F>
    holdable_signal_connection(T& signal, F&& f) : base(signal, std::forward<F>(f)) {}

    void call_(tuple_type& arg) override {
        if (this->blocked()) {
            args_.push_back(arg);
        } else {
            ctl::tuple_to_args(this->callback, arg);
        }
    }

public :
    void unblock() override {
        base::unblock();

        if (args_.empty()) return;

        for (auto& arg : args_) {
            ctl::tuple_to_args(this->callback, arg);
        }

        args_.clear();
    }
};

/// Same as holdable_signal_connection, but will only call the slot once, then stop.
template<typename T>
class unique_holdable_signal_connection : public basic_signal_connection<T> {
protected :
    using base = basic_signal_connection<T>;
    using delegate_type = typename T::delegate_type;
    using tuple_type = typename T::tuple_type;

    using typename base::deleter;
    friend T;

    std::unique_ptr<tuple_type> arg_;
    bool received_ = false;

    template<typename F>
    unique_holdable_signal_connection(T& signal, F&& f) : base(signal, std::forward<F>(f)) {}

    void call_(tuple_type& arg) override {
        if (received_) return;
        if (this->blocked()) {
            arg_ = std::make_unique<tuple_type>(arg);
            received_ = true;
        } else {
            ctl::tuple_to_args(this->callback, arg);
            this->stop();
        }
    }

public :
    void unblock() override {
        base::unblock();

        if (!arg_) return;

        ctl::tuple_to_args(this->callback, *arg_);
        arg_ = nullptr;

        this->stop();
    }
};

#endif
