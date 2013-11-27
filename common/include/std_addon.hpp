#ifndef STD_ADDON_HPP
#define STD_ADDON_HPP

#include <memory>

namespace std {
    template<typename T, typename... Args>
    std::unique_ptr<T> make_unique(Args&&... args) {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
}

template<typename T>
struct range_t {
    std::pair<T,T> p;
    range_t(const std::pair<T,T>& p) : p(p) {}
    T begin() { return p.first;  }
    T end()   { return p.second; }
};

template<typename T>
range_t<T> range(const std::pair<T,T>& p) { return range_t<T>(p); }

#endif
