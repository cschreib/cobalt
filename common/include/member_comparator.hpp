#ifndef MEMBER_COMPARATOR_HPP
#define MEMBER_COMPARATOR_HPP

#include <memory>
#include "variadic.hpp"

template<typename T, typename I, I (T::*F)() const>
struct mem_fun_comp_ {
    using id_t = typename std::remove_cv<I>::type;

    bool operator () (const T& t1, const T& t2) const {
        return (t1.*F)() < (t2.*F)();
    }

    bool operator () (const T& t1, const id_t& t2) const {
        return (t1.*F)() < t2;
    }

    bool operator () (const id_t& t1, const T& t2) const {
        return t1 < (t2.*F)();
    }

    bool operator () (const T* t1, const T* t2) const {
        return (*t1.*F)() < (*t2.*F)();
    }

    bool operator () (const T* t1, const id_t& t2) const {
        return (*t1.*F)() < t2;
    }

    bool operator () (const id_t& t1, const T* t2) const {
        return t1 < (*t2.*F)();
    }

    bool operator () (const std::unique_ptr<T>& t1, const std::unique_ptr<T>& t2) const {
        return (*t1.*F)() < (*t2.*F)();
    }

    bool operator () (const std::unique_ptr<T>& t1, const id_t& t2) const {
        return (*t1.*F)() < t2;
    }

    bool operator () (const id_t& t1, const std::unique_ptr<T>& t2) const {
        return t1 < (*t2.*F)();
    }
};

template<typename T>
struct mem_fun_traits;

template<typename T, typename I> 
struct mem_fun_traits<I (T::*)()> {
    using owner = T;
    using return_type = I;
};

template<typename T, typename I> 
struct mem_fun_traits<I (T::*)() const> {
    using owner = T;
    using return_type = I;
};

#define mem_fun_comp(f) mem_fun_comp_< \
    typename mem_fun_traits<decltype(f)>::owner, \
    typename mem_fun_traits<decltype(f)>::return_type, \
    f \
>

template<typename T, typename I, I T::*V>
struct mem_var_comp_ {
    using id_t = typename std::remove_cv<I>::type;

    bool operator () (const T& t1, const T& t2) const {
        return t1.*V < t2.*V;
    }

    bool operator () (const T& t1, const id_t& t2) const {
        return t1.*V < t2;
    }

    bool operator () (const id_t& t1, const T& t2) const {
        return t1 < t2.*V;
    }

    bool operator () (const T* t1, const T* t2) const {
        return *t1.*V < *t2.*V;
    }

    bool operator () (const T* t1, const id_t& t2) const {
        return *t1.*V < t2;
    }

    bool operator () (const id_t& t1, const T* t2) const {
        return t1 < *t2.*V;
    }

    bool operator () (const std::unique_ptr<T>& t1, const std::unique_ptr<T>& t2) const {
        return *t1.*V < *t2.*V;
    }

    bool operator () (const std::unique_ptr<T>& t1, const id_t& t2) const {
        return *t1.*V < t2;
    }

    bool operator () (const id_t& t1, const std::unique_ptr<T>& t2) const {
        return t1 < *t2.*V;
    }
};

template<typename T>
struct mem_var_traits;

template<typename T, typename I> 
struct mem_var_traits<I T::*> {
    using owner = T;
    using type = I;
};

#define mem_var_comp(v) mem_var_comp_< \
    typename mem_var_traits<decltype(v)>::owner, \
    typename mem_var_traits<decltype(v)>::type, \
    v \
>

#endif
