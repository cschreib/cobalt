#ifndef DEFAULT_COMPARATOR_HPP
#define DEFAULT_COMPARATOR_HPP

#include <memory>

template<typename T>
struct default_comparator {
    using type = std::less<T>;
};

template<typename T>
struct default_comparator<T&> {
    struct type {
        bool operator () (const T* t1, const T* t2) const {
            return std::less<const T*>()(t1, t2);
        }
        bool operator () (const T& t1, const T& t2) const {
            return std::less<const T*>()(&t1, &t2);
        }
    };
};

template<typename T>
struct default_comparator<std::unique_ptr<T>> {
    struct type {
        bool operator () (const std::unique_ptr<T>& t1, const std::unique_ptr<T>& t2) const {
            return std::less<const T*>()(t1.get(), t2.get());
        }
        bool operator () (const std::unique_ptr<T>& t1, const T* t2) const {
            return std::less<const T*>()(t1.get(), t2);
        }
        bool operator () (const T* t1, const std::unique_ptr<T>& t2) const {
            return std::less<const T*>()(t1, t2.get());
        }
    };
};

#endif
