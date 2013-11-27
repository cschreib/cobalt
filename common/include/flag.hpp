#ifndef FLAG_HPP
#define FLAG_HPP

#include <utility>
#include "variadic.hpp"

namespace flag_impl {
    template<typename Stor, typename ... Args>
    struct get_first_impl;

    template<typename ... Args1, typename T, typename ... Args2>
    struct get_first_impl<type_list<Args1...>, T, Args2...> {
        using type = typename get_first_impl<type_list<Args1..., T>, Args2...>::type;
    };

    template<typename ... Args1, typename ... Args2>
    struct get_first_impl<type_list<Args1...>, separator, Args2...> {
        using type = type_list<Args1...>;
    };

    template<typename ... Args1, typename T>
    struct get_first_impl<type_list<Args1...>, T> {
        using type = type_list<Args1..., T>;
    };

    template<typename ... Args>
    using get_first = get_first_impl<type_list<>, Args...>;

    template<bool found, typename Stor, typename ... Args>
    struct get_second_impl;

    template<typename T, typename ... Args2>
    struct get_second_impl<false, type_list<>, T, Args2...> {
        using type = typename get_second_impl<false, type_list<>, Args2...>::type;
    };

    template<typename ... Args2>
    struct get_second_impl<false, type_list<>, separator, Args2...> {
        using type = typename get_second_impl<true, type_list<>, Args2...>::type;
    };

    template<typename ... Args1, typename T, typename ... Args2>
    struct get_second_impl<true, type_list<Args1...>, T, Args2...> {
        using type = typename get_second_impl<true, type_list<Args1..., T>, Args2...>::type;
    };

    template<typename ... Args1, typename T>
    struct get_second_impl<true, type_list<Args1...>, T> {
        using type = type_list<Args1..., T>;
    };

    template<typename T>
    struct get_second_impl<false, type_list<>, T> {
        using type = type_list<>;
    };

    template<typename ... Args>
    using get_second = get_second_impl<false, type_list<>, Args...>;

    template<typename Test, typename Check>
    struct are_any_of_impl;

    template<typename T, typename ... Test, typename ... Check>
    struct are_any_of_impl<type_list<T, Test...>, type_list<Check...>> {
        static const bool value =
            is_any_of<T, Check...>::value &&
            are_any_of_impl<type_list<Test...>, type_list<Check...>>::value;
    };

    template<typename T, typename ... Check>
    struct are_any_of_impl<type_list<T>, type_list<Check...>> {
        static const bool value = is_any_of<T, Check...>::value;
    };

    template<typename ... Args>
    using are_any_of = are_any_of_impl<typename get_first<Args...>::type, typename get_second<Args...>::type>;

    template<std::size_t N, typename T, typename ... Args>
    struct get_pos_impl;
    template<std::size_t N, typename T, typename U, typename ... Args>
    struct get_pos_impl<N, T, U, Args...> {
        static const std::size_t value = std::is_same<T, U>::value ? N : get_pos_impl<N+1, T, Args...>::value;
    };
    template<std::size_t N, typename T, typename U>
    struct get_pos_impl<N, T, U> {
        static const std::size_t value = std::is_same<T, U>::value ? N : N+1;
    };
    template<typename T, typename ... Args>
    using get_pos = get_pos_impl<0, T, Args...>;
}

template<typename ... Args>
struct flag_set {
    constexpr flag_set() {}

    template<typename ... Args2, typename enable = typename std::enable_if<flag_impl::are_any_of<Args2..., flag_impl::separator, Args...>::value>::type>
    explicit constexpr flag_set(Args2&& ... args) : value(build(0, std::forward<Args2>(args)...)) {
    }

    constexpr flag_set(const flag_set& f) : value(f.value) {}

    template<typename T, typename ... Args2, typename enable = typename std::enable_if<flag_impl::are_any_of<T, Args2..., flag_impl::separator, Args...>::value>::type>
    void set(T t, Args2&& ... args) {
        value |= static_cast<int>(t);
        set(std::forward<Args2>(args)...);
    }

    template<typename T, typename enable = typename std::enable_if<flag_impl::is_any_of<T, Args...>::value>::type>
    void set(T t) {
        value |= static_cast<int>(t);
    }

    template<typename T, typename enable = typename std::enable_if<flag_impl::is_any_of<T, Args...>::value>::type>
    flag_set& operator << (T t) {
        set(t);
        return *this;
    }

    template<typename T, typename enable = typename std::enable_if<flag_impl::is_any_of<T, Args...>::value>::type>
    bool operator == (T t) {
        return ((value & (1 << flag_impl::get_pos<T, Args ...>::value)) == static_cast<int>(t));
    }

    template<typename T, typename enable = typename std::enable_if<flag_impl::is_any_of<T, Args...>::value>::type>
    bool operator != (T t) {
        return ((value & (1 << flag_impl::get_pos<T, Args ...>::value)) != static_cast<int>(t));
    }

    int value = 0;

private :

    template<typename T, typename ... Args2>
    static constexpr int build(int v, T t, Args2&& ... args) {
        return build(v | static_cast<int>(t), std::forward<Args2>(args)...);
    }

    template<typename T>
    static constexpr int build(int v, T t) {
        return v | static_cast<int>(t);
    }
};

#endif
