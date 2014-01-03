#ifndef VARIADIC_HPP
#define VARIADIC_HPP

#include <type_traits>
#include <tuple>

/// Cobalt template library
namespace ctl {
    /// value == all the provided types have T::value == true
    template<typename ... Args>
    struct are_true;

    template<typename T, typename ... Args>
    struct are_true<T, Args...> {
        static const bool value = T::value && are_true<Args...>::value;
    };

    template<>
    struct are_true<> : std::true_type {};


    template<typename T, typename ... Args>
    struct is_any_of;
    template<typename T, typename U, typename ... Args>
    struct is_any_of<T, U, Args...> {
        static const bool value = std::is_same<T, U>::value || is_any_of<T, Args...>::value;
    };
    template<typename T, typename U>
    struct is_any_of<T, U> {
        static const bool value = std::is_same<T, U>::value;
    };

    struct separator {};

    template<typename ... Args>
    struct head_t;
    template<typename T, typename ... Args>
    struct head_t<T, Args...> {
        using type = T;
    };
    template<typename ... Args>
    using head = typename head_t<Args...>::type;


    template<typename ... Args>
    struct tail_t;
    template<typename T>
    struct tail_t<T> {
        using type = T;
    };
    template<typename T, typename ... Args>
    struct tail_t<T, Args...> {
        using type = typename tail_t<Args...>::type;
    };
    template<typename ... Args>
    using tail = typename tail_t<Args...>::type;

    template<typename ... Args>
    struct type_list;

    template<typename ... Args>
    struct type_list_popf_t;
    template<typename T, typename ... Args>
    struct type_list_popf_t<T, Args...> {
        using type = type_list<Args...>;
    };

    template<typename ... Args>
    struct type_list_popb_t;
    template<typename T>
    struct type_list_popb_t<T> {
        using type = T;
    };
    template<typename T, typename ... Args>
    struct type_list_popb_t<T, Args...> {
        using type = typename type_list_popb_t<Args...>::type;
    };

    template<>
    struct type_list<> {
        template<typename V>
        using push_front_t = type_list<V>;
        template<typename V>
        using push_back_t = type_list<V>;

        template<typename V>
        push_front_t<V> push_front() { return push_front_t<V>(); }
        template<typename V>
        push_back_t<V> push_back() { return push_back_t<V>(); }
    };

    template<typename T>
    struct type_list<T> {
        template<typename V>
        using push_front_t = type_list<V, T>;
        template<typename V>
        using push_back_t = type_list<T, V>;

        using pop_front_t = type_list<>;
        using pop_back_t = type_list<>;

        using front_t = T;
        using back_t = T;

        template<typename V>
        push_front_t<V> push_front() { return push_front_t<V>(); }
        template<typename V>
        push_back_t<V> push_back() { return push_back_t<V>(); }
        pop_front_t pop_front() { return pop_front_t(); }
        pop_back_t pop_back() { return pop_back_t(); }
    };

    template<typename ... Args>
    struct type_list {
        template<typename V>
        using push_front_t = type_list<V, Args...>;
        template<typename V>
        using push_back_t = type_list<Args..., V>;

        using pop_front_t = typename type_list_popf_t<Args...>::type;
        using pop_back_t = typename type_list_popb_t<Args...>::type;

        using front_t = head<Args...>;
        using back_t = tail<Args...>;

        template<typename V>
        push_front_t<V> push_front() { return push_front_t<V>(); }
        template<typename V>
        push_back_t<V> push_back() { return push_back_t<V>(); }
        pop_front_t pop_front() { return pop_front_t(); }
        pop_back_t pop_back() { return pop_back_t(); }
    };

    template<std::size_t N, typename T, typename ... Args>
    struct type_list_element__t {
        using type = typename type_list_element__t<N-1, Args...>::type;
    };

    template<typename T, typename ... Args>
    struct type_list_element__t<0, T, Args...> {
        using type = T;
    };

    template<std::size_t N, typename T>
    struct type_list_element_t;

    template<std::size_t N, typename ... Args>
    struct type_list_element_t<N, type_list<Args...>> {
        using type = typename type_list_element__t<N, Args...>::type;
    };

    template<std::size_t N, typename T>
    using type_list_element = typename type_list_element_t<N, T>::type;

    template<typename T>
    struct type_list_size;

    template<typename ... Args>
    struct type_list_size<type_list<Args...>> {
        static const std::size_t value = sizeof...(Args);
    };

    template<typename ... Args, typename ... TArgs>
    struct are_true<type_list<Args...>, TArgs...> {
        static const bool value = are_true<Args...>::value && are_true<TArgs...>::value;
    };

    namespace impl {
        template<template<typename> class A, typename T>
        struct apply_to_type_list_;

        template<template<typename> class A, typename T, typename ... Args>
        struct apply_to_type_list_<A, type_list<T, Args...>> {
            using single = typename A<T>::type;
            using base = typename apply_to_type_list_<A, type_list<Args...>>::type;
            using type = typename base::template push_front_t<single>;
        };

        template<template<typename> class A>
        struct apply_to_type_list_<A, type_list<>> {
            using type = type_list<>;
        };
    }

    template<template<typename> class A, typename T>
    using apply_to_type_list = typename impl::apply_to_type_list_<A,T>::type;

    template<typename Pack1, typename Pack2>
    struct are_convertible;

    template<typename T1, typename T2,typename ... Args1, typename ... Args2>
    struct are_convertible<type_list<T1, Args1...>, type_list<T2, Args2...>> {
        static const bool value = std::is_convertible<T1,T2>::value &&
            are_convertible<type_list<Args1...>, type_list<Args2...>>::value;
    };

    template<>
    struct are_convertible<type_list<>, type_list<>> {
        static const bool value = true;
    };

    template<typename T>
    struct type_holder {
        using type = T;
    };

    template<typename T>
    struct type_holder_get_type;

    template<typename T>
    struct type_holder_get_type<type_holder<T>> {
        using type = T;
    };

    template<typename T>
    struct function_arguments_t;

    template<typename R, typename T, typename ... Args>
    struct function_arguments_t<R (T::*)(Args...)> {
        using type = type_list<Args...>;
    };

    template<typename R, typename T, typename ... Args>
    struct function_arguments_t<R (T::*)(Args...) const> {
        using type = type_list<Args...>;
    };

    template<typename R, typename ... Args>
    struct function_arguments_t<R (*)(Args...)> {
        using type = type_list<Args...>;
    };

    template<typename R, typename ... Args>
    struct function_arguments_t<R (Args...)> {
        using type = type_list<Args...>;
    };

    template<typename T>
    using function_arguments = typename function_arguments_t<T>::type;

    template<typename T>
    using functor_arguments = function_arguments<decltype(&T::operator())>;

    template<typename T>
    struct function_argument_t;

    template<typename R, typename T, typename Arg>
    struct function_argument_t<R (T::*)(Arg)> {
        using type = Arg;
    };

    template<typename R, typename T, typename Arg>
    struct function_argument_t<R (T::*)(Arg) const> {
        using type = Arg;
    };

    template<typename T>
    using function_argument = typename function_argument_t<T>::type;

    template<typename T>
    using functor_argument = function_argument<decltype(&T::operator())>;

    template<typename T>
    struct argument_count {
        static const std::size_t value = argument_count<decltype(&T::operator())>::value;
    };

    template<typename R, typename T, typename ... Args>
    struct argument_count<R (T::*)(Args...)> {
        static const std::size_t value = sizeof...(Args);
    };

    template<typename R, typename T, typename ... Args>
    struct argument_count<R (T::*)(Args...) const> {
        static const std::size_t value = sizeof...(Args);
    };

    template<typename T>
    struct return_type;

    template<typename R, typename ... Args>
    struct return_type<R(Args...)> {
        using type = R;
    };

    template<typename R, typename ... Args>
    struct return_type<R(*)(Args...)> {
        using type = R;
    };

    template<typename R, typename T, typename ... Args>
    struct return_type<R(T::*)(Args...)> {
        using type = R;
    };

    template<typename R, typename T, typename ... Args>
    struct return_type<R(T::*)(Args...) const> {
        using type = R;
    };

    auto no_op = [](){};

    template<typename T1, typename T2>
    struct has_left_shift {
        template <typename U> static std::true_type  dummy(typename std::decay<
            decltype(std::declval<T1&>() << std::declval<const U&>())>::type*);
        template <typename U> static std::false_type dummy(...);
        static const bool value = decltype(dummy<T2>(0))::value;
    };

    template<typename T1, typename T2>
    struct has_right_shift {
        template <typename U> static std::true_type  dummy(typename std::decay<
            decltype(std::declval<T1&>() << std::declval<const U&>())>::type*);
        template <typename U> static std::false_type dummy(...);
        static const bool value = decltype(dummy<T2>(0))::value;
    };


    /// Class holding a sequence of integer values.
    template<std::size_t ...>
    struct seq_t {};

    namespace impl {
        template<std::size_t N, std::size_t D, std::size_t ... S>
        struct gen_seq_ : public gen_seq_<N+1, D, S..., N> {};

        template<std::size_t D, std::size_t ... S>
        struct gen_seq_<D, D, S...> {
          using type = seq_t<S..., D>;
        };
    }

    /// Generate a sequence of integer from 0 to D (exclusive).
    template<std::size_t D>
    using gen_seq = typename impl::gen_seq_<0, D-1>::type;


    /// Get the return type of a functor given the parameter types
    template<typename F, typename ... Args>
    struct result_of_functor {
        using type = decltype(std::declval<F>()(std::declval<Args>()...));
    };


    namespace impl {
        template<typename F, typename T>
        struct t2a_return_;

        template<typename F, typename ... Args>
        struct t2a_return_<F, std::tuple<Args...>> {
            using type = typename result_of_functor<F, Args...>::type;
        };

        template<typename F, typename T>
        using t2a_return = typename t2a_return_<
            typename std::decay<F>::type,
            typename std::decay<T>::type
        >::type;

        template<typename F, typename T, std::size_t ... I>
        t2a_return<F,T> tuple_to_args_(F&& func, T&& tup, seq_t<I...>) {
            return func(std::get<I>(std::forward<T>(tup))...);
        }

        template<typename F, typename T, std::size_t N>
        t2a_return<F,T> tuple_to_args_(F&& func, T&& tup,
            std::integral_constant<std::size_t,N>) {
            return impl::tuple_to_args_(
                std::forward<F>(func), std::forward<T>(tup), gen_seq<N>{}
            );
        }

        template<typename F, typename T>
        t2a_return<F,T> tuple_to_args_(F&& func, T&& tup,
            std::integral_constant<std::size_t,0>) {
            return func();
        }

        template<typename F, typename T, std::size_t ... I>
        t2a_return<F,T> tuple_to_moved_args_(F&& func, T&& tup, seq_t<I...>) {
            return func(std::move(std::get<I>(tup))...);
        }

        template<typename F, typename T, std::size_t N>
        t2a_return<F,T> tuple_to_moved_args_(F&& func, T&& tup,
            std::integral_constant<std::size_t,N>) {
            return impl::tuple_to_moved_args_(
                std::forward<F>(func), std::move(tup), gen_seq<N>{}
            );
        }

        template<typename F, typename T>
        t2a_return<F,T> tuple_to_moved_args_(F&& func, T&& tup,
            std::integral_constant<std::size_t,0>) {
            return func();
        }
    }

    /// Unfold a tuple and use the individual elements as function parameters
    template<typename F, typename T>
    impl::t2a_return<F,T> tuple_to_args(F&& func, T&& tup) {
        using tuple_type = typename std::decay<T>::type;
        return impl::tuple_to_args_(
            std::forward<F>(func), std::forward<T>(tup),
            std::integral_constant<std::size_t, std::tuple_size<tuple_type>::value>{}
        );
    }

    /// Unfold a tuple and move the individual elements as function parameters
    template<typename F, typename ... Args>
    impl::t2a_return<F,std::tuple<Args&&...>> tuple_to_moved_args(F&& func,
        std::tuple<Args...>&& tup) {
        using tuple_type = std::tuple<Args...>;
        return impl::tuple_to_moved_args_(
            std::forward<F>(func), std::move(tup),
            std::integral_constant<std::size_t, std::tuple_size<tuple_type>::value>{}
        );
    }
}

#endif
