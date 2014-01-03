#ifndef HOLDABLE_SIGNAL_CONNECTION_HPP
#define HOLDABLE_SIGNAL_CONNECTION_HPP

#include "signal.hpp"

namespace signal_impl {
    template<typename T>
    using make_stored_tuple_arg = typename std::decay<T>::type;

    template<typename T>
    struct make_stored_tuple_type_;

    template<typename ... Args>
    struct make_stored_tuple_type_<std::tuple<Args...>> {
        using type = std::tuple<make_stored_tuple_arg<Args>...>;
    };

    template<typename T>
    using make_stored_tuple_type = typename make_stored_tuple_type_<T>::type;

    template<typename T>
    struct vector_emplace_back_wrapper_t {
        std::vector<T>& v;

        template<typename ... Args>
        void operator() (Args&& ... args) {
            v.emplace_back(std::forward<Args>(args)...);
        }
    };

    template<typename T>
    vector_emplace_back_wrapper_t<T> vector_emplace_back_wrapper(std::vector<T>& v) {
        return vector_emplace_back_wrapper_t<T>{v};
    }

    template<typename T>
    struct make_unique_wrapper {
        template<typename ... Args>
        std::unique_ptr<T> operator() (Args&& ... args) {
            return std::make_unique<T>(std::forward<Args>(args)...);
        }
    };
}

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
    using signature = typename T::signature;
    using tuple_type = typename T::tuple_type;
    using arg_type = typename T::arg_type;
    using stored_tuple_type = signal_impl::make_stored_tuple_type<tuple_type>;
    using allow_arg_move = typename T::allow_arg_move;
    using can_copy_construct = typename T::can_copy_construct;
    using can_move_construct = typename T::can_move_construct;

    static_assert(can_copy_construct::value || allow_arg_move::value,
        "signal arguments must be copiable to be used by this holdable_signal_connection");

    static_assert(can_move_construct::value || can_copy_construct::value || !allow_arg_move::value,
        "signal arguments must be movable or copiable to be used by this holdable_signal_connection");

    std::vector<stored_tuple_type> args_;

    using typename base::deleter;
    friend T;

    template<typename F>
    holdable_signal_connection(T& signal, F&& f) : base(signal, std::forward<F>(f)) {}

    void call_(arg_type arg) override {
        if (this->blocked()) {
            this->do_call_(
                signal_impl::vector_emplace_back_wrapper(args_),
                std::forward<arg_type>(arg)
            );
        } else {
            this->do_call_(std::forward<arg_type>(arg));
        }
    }

    using base::do_call_;

    template<typename ... Args>
    void do_call_(std::tuple<Args...>& arg, std::false_type) {
        this->do_call_(arg);
    }

    template<typename ... Args>
    void do_call_(std::tuple<Args...>& arg, std::true_type) {
        this->do_call_(std::move(arg));
    }

public :
    void unblock() override {
        base::unblock();

        if (args_.empty()) return;

        for (auto& arg : args_) {
            this->do_call_(arg, allow_arg_move{});
        }

        args_.clear();
    }
};

/// Same as holdable_signal_connection, but will only call the slot once, then stop.
template<typename T>
class unique_holdable_signal_connection : public basic_signal_connection<T> {
protected :
    using base = basic_signal_connection<T>;
    using signature = typename T::signature;
    using tuple_type = typename T::tuple_type;
    using arg_type = typename T::arg_type;
    using stored_tuple_type = signal_impl::make_stored_tuple_type<tuple_type>;
    using allow_arg_move = typename T::allow_arg_move;
    using can_copy_construct = typename T::can_copy_construct;
    using can_move_construct = typename T::can_move_construct;

    static_assert(can_copy_construct::value || allow_arg_move::value,
        "signal arguments must be copiable to be used by this holdable_signal_connection");

    static_assert(can_move_construct::value || can_copy_construct::value || !allow_arg_move::value,
        "signal arguments must be movable or copiable to be used by this holdable_signal_connection");

    using typename base::deleter;
    friend T;

    std::unique_ptr<stored_tuple_type> arg_;
    bool received_ = false;

    template<typename F>
    unique_holdable_signal_connection(T& signal, F&& f) : base(signal, std::forward<F>(f)) {}

    void call_(arg_type arg) override {
        if (received_) return;
        if (this->blocked()) {
            arg_ = this->do_call_(
                signal_impl::make_unique_wrapper<stored_tuple_type>{},
                std::forward<arg_type>(arg)
            );
            received_ = true;
        } else {
            ctl::tuple_to_args(this->callback, std::forward<arg_type>(arg));
            this->stop();
        }
    }

    void forward_arg_(stored_tuple_type& arg, std::false_type) {
        ctl::tuple_to_args(this->callback, arg);
    }

    void forward_arg_(stored_tuple_type& arg, std::true_type) {
        ctl::tuple_to_args(this->callback, std::move(arg));
    }

public :
    void unblock() override {
        base::unblock();

        if (!arg_) return;

        forward_arg_(*arg_, allow_arg_move{});
        arg_ = nullptr;

        this->stop();
    }
};

#endif
