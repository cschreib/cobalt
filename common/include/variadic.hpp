#ifndef VARIADIC_HPP
#define VARIADIC_HPP

#include <type_traits>
#include <tuple>

/// Cobalt template library
namespace ctl {
    /// Empty type
    struct empty_t {};


    /// Type list
    template<typename ... Args>
    struct type_list {};

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

    template<typename T>
    struct tuple_to_type_list_;

    template<typename ... Args>
    struct tuple_to_type_list_<std::tuple<Args...>> {
        using type = type_list<Args...>;
    };

    template<typename T>
    using tuple_to_type_list = typename tuple_to_type_list_<typename std::decay<T>::type>::type;


    /// value == true if all the provided types have T::value == true
    template<typename ... Args>
    struct are_true;

    template<>
    struct are_true<> : std::true_type {};

    template<typename T, typename ... Args>
    struct are_true<T, Args...> :
        std::integral_constant<bool, T::value && are_true<Args...>::value> {};

    template<typename ... Args, typename ... TArgs>
    struct are_true<type_list<Args...>, TArgs...> : std::integral_constant<bool,
        are_true<Args...>::value && are_true<TArgs...>::value> {};


    /// value == true if any of the provided types have T::value == true
    template<typename T, typename ... Args>
    struct is_any_type_of;

    template<typename T, typename U, typename ... Args>
    struct is_any_type_of<T, U, Args...> {
        static const bool value = std::is_same<T, U>::value || is_any_type_of<T, Args...>::value;
    };

    template<typename T, typename U>
    struct is_any_type_of<T, U> {
        static const bool value = std::is_same<T, U>::value;
    };

    template<typename T, typename ... Args>
    struct is_any_type_of<T, type_list<Args...>> {
        static const bool value = is_any_type_of<T, Args...>::value;
    };


    /// value == true if all the types in the first list are found among the second list
    template<typename Test, typename Check>
    struct are_any_type_of;

    template<typename T, typename ... Test, typename ... Check>
    struct are_any_type_of<type_list<T, Test...>, type_list<Check...>> {
        static const bool value =
            is_any_type_of<T, Check...>::value &&
            are_any_type_of<type_list<Test...>, type_list<Check...>>::value;
    };

    template<typename T, typename ... Check>
    struct are_any_type_of<type_list<T>, type_list<Check...>> {
        static const bool value = is_any_type_of<T, Check...>::value;
    };


    /// Find position of type in type list
    namespace impl {
        template<std::size_t N, typename T, typename ... Args>
        struct get_position_in_type_list_impl;

        template<std::size_t N, typename T, typename U, typename ... Args>
        struct get_position_in_type_list_impl<N, T, U, Args...> {
            static const std::size_t value = std::is_same<T, U>::value ? N : get_position_in_type_list_impl<N+1, T, Args...>::value;
        };

        template<std::size_t N, typename T, typename U>
        struct get_position_in_type_list_impl<N, T, U> {
            static const std::size_t value = std::is_same<T, U>::value ? N : N+1;
        };

        template<typename T, typename ... Args>
        struct get_position_in_type_list_impl<0, T, type_list<Args...>> {
            static const std::size_t value = get_position_in_type_list_impl<0, T, Args...>::value;
        };
    }

    template<typename T, typename ... Args>
    using get_position_in_type_list = impl::get_position_in_type_list_impl<0, T, Args...>;


    /// Apply template function to a type list
    namespace impl {
        template<template<typename> class A, typename T>
        struct apply_to_type_list_;

        template<template<typename> class A, typename ... Args>
        struct apply_to_type_list_<A, type_list<Args...>> {
            using type = type_list<A<Args>...>;
        };
    }

    template<template<typename> class A, typename T>
    using apply_to_type_list = typename impl::apply_to_type_list_<A,T>::type;


    /// Check if all types in the first list can be converted to their matching type in the other list
    template<typename Pack1, typename Pack2>
    struct are_convertible;

    template<typename ... Args1, typename ... Args2>
    struct are_convertible<type_list<Args1...>, type_list<Args2...>> :
        are_true<std::is_convertible<Args1, Args2>...> {};

    template<>
    struct are_convertible<type_list<>, type_list<>> {
        static const bool value = true;
    };


    /// Get normalized signature from function pointer or member function
    namespace impl {
        template<typename T>
        struct get_signature_t;

        template<typename R, typename ... Args>
        struct get_signature_t<R(Args...)> {
            using type = R(Args...);
        };

        template<typename R, typename ... Args>
        struct get_signature_t<R(*)(Args...)> {
            using type = R(Args...);
        };

        template<typename R, typename T, typename ... Args>
        struct get_signature_t<R(T::*)(Args...)> {
            using type = R(Args...);
        };

        template<typename R, typename T, typename ... Args>
        struct get_signature_t<R(T::*)(Args...) const> {
            using type = R(Args...);
        };
    }

    template<typename T>
    using get_signature = typename impl::get_signature_t<typename std::decay<T>::type>::type;


    /// Get the list of argument types from a function signature
    namespace impl {
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
        struct function_arguments_t : function_arguments_t<decltype(&T::operator())> {};
    }

    template<typename T>
    using function_arguments = typename impl::function_arguments_t<typename std::decay<T>::type>::type;


    /// For a function signature with a single argument, return the type of this argument
    namespace impl {

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
        struct function_argument_t : function_argument_t<decltype(&T::operator())> {};
    }

    template<typename T>
    using function_argument = typename impl::function_argument_t<typename std::decay<T>::type>::type;


    /// Get number of arguments in a function signature
    namespace impl {
        template<typename T>
        struct argument_count_;

        template<typename R, typename T, typename ... Args>
        struct argument_count_<R (T::*)(Args...)> :
            std::integral_constant<std::size_t, sizeof...(Args)> {};

        template<typename R, typename T, typename ... Args>
        struct argument_count_<R (T::*)(Args...) const> :
            std::integral_constant<std::size_t, sizeof...(Args)> {};

        template<typename T>
        struct argument_count_ : argument_count_<decltype(&T::operator())> {};
    }

    template<typename T>
    using argument_count = impl::argument_count_<typename std::decay<T>::type>;


    /// Get return type from function signature
    namespace impl {
        template<typename T>
        struct return_type_t;

        template<typename T>
        struct return_type_t : return_type_t<decltype(&T::operator())> {};

        template<typename R, typename ... Args>
        struct return_type_t<R(Args...)> {
            using type = R;
        };

        template<typename R, typename ... Args>
        struct return_type_t<R(*)(Args...)> {
            using type = R;
        };

        template<typename R, typename T, typename ... Args>
        struct return_type_t<R(T::*)(Args...)> {
            using type = R;
        };

        template<typename R, typename T, typename ... Args>
        struct return_type_t<R(T::*)(Args...) const> {
            using type = R;
        };
    }

    template<typename T>
    using return_type = typename impl::return_type_t<typename std::decay<T>::type>::type;


    /// Check if type has << or >> operator overloaded
    namespace impl {
        template<typename T1, typename T2>
        struct has_left_shift_t {
            template <typename U> static std::true_type  dummy(typename std::decay<
                decltype(std::declval<T1&>() << std::declval<const U&>())>::type*);
            template <typename U> static std::false_type dummy(...);
            using type = decltype(dummy<T2>(0));
        };

        template<typename T1, typename T2>
        struct has_right_shift_t {
            template <typename U> static std::true_type  dummy(typename std::decay<
                decltype(std::declval<T1&>() >> std::declval<U&>())>::type*);
            template <typename U> static std::false_type dummy(...);
            using type = decltype(dummy<T2>(0));
        };
    }

    template<typename T1, typename T2>
    using has_left_shift = typename impl::has_left_shift_t<T1,T2>::type;

    template<typename T1, typename T2>
    using has_right_shift = typename impl::has_right_shift_t<T1,T2>::type;


    /// Unfold a tuple and use the individual elements as function parameters
    namespace impl {
        template<typename F, typename T, typename ... Args, std::size_t ... I>
        auto tuple_to_args_(F&& func, T&& tup, std::index_sequence<I...>) {
            return std::forward<F>(func)(std::get<I>(std::forward<T>(tup))...);
        }
    }

    template<typename F, typename T>
    auto tuple_to_args(F&& func, T&& tup) {
        using tuple_type = typename std::decay<T>::type;
        using seq_type = std::make_index_sequence<std::tuple_size<tuple_type>::value>;
        return impl::tuple_to_args_(std::forward<F>(func), std::forward<T>(tup), seq_type{});
    }
}

#endif
