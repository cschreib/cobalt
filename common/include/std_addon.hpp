#ifndef STD_ADDON_HPP
#define STD_ADDON_HPP

#include <memory>
#include <algorithm>

namespace ctl {
    // Adapter to iterate over a range returned as std::pair<iterator>
    template<typename T>
    struct range_t {
        std::pair<T,T> p;
        range_t(const std::pair<T,T>& p) : p(p) {}
        T begin() { return p.first;  }
        T end()   { return p.second; }
    };

    template<typename T>
    range_t<T> range(const std::pair<T,T>& p) { return range_t<T>(p); }

    // Shortcut for std::remove_if
    template<typename C, typename F>
    void erase_if(C& cont, F&& func) {
        cont.erase(std::remove_if(cont.begin(), cont.end(), std::forward<F>(func)), cont.end());
    }
}

#endif
