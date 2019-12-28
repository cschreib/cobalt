#ifndef FLAG_HPP
#define FLAG_HPP

#include <utility>
#include "variadic.hpp"

template<typename ... Args>
struct flag_set {
    using flags = ctl::type_lists<Args...>;

    template<typename T>
    static constexpr int get_flag_value() {
        return 1 << ctl::get_position_in_type_list<T, flags>::value;
    }

    template<typename T, typename enable = typename std::enable_if<ctl::is_any_of<T, flags>::value>::type>
    void set() {
        value |= get_flag_value<T>();
    }

    template<typename T, typename ... Args2, typename enable = typename std::enable_if<ctl::are_any_type_of<ctl::type_list<T, Args2...>, flags>::value>::type>
    void set() {
        set<T>();
        set(std::forward<Args2>(args)...);
    }

    template<typename T, typename enable = typename std::enable_if<ctl::is_any_of<T, flags>::value>::type>
    bool is_set(T t) {
        return (value & get_flag_value<T>()) != 0;
    }

private:

    int value = 0;
};

#endif
