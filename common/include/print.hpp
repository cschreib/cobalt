#ifndef PRINT_HPP
#define PRINT_HPP

#include <string>
#include <iostream>

template<typename T>
void print(const T& t) {
    std::cout << t << std::endl;
}

template<typename T, typename ... Args>
void print(const T& t, Args&&... args) {
    std::cout << t;
    print(std::forward<Args>(args)...);
}

template<typename ... Args>
void error(Args&& ... args) {
    print("error: ", std::forward<Args>(args)...);
}

template<typename ... Args>
void warning(Args&& ... args) {
    print("warning: ", std::forward<Args>(args)...);
}

template<typename ... Args>
void note(Args&& ... args) {
    print("note: ", std::forward<Args>(args)...);
}

template<typename ... Args>
void reason(Args&& ... args) {
    print("reason: ", std::forward<Args>(args)...);
}

#endif

