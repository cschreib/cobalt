#ifndef ITERATOR_BASE_HPP
#define ITERATOR_BASE_HPP

#include <iterator>

template<typename T, typename I>
class ptr_iterator_base_impl {
protected :
    I i;

    ptr_iterator_base_impl(const I& j) : i(j) {}

public :
    T& operator ++ (int) {
        ++i; return static_cast<T&>(*this);
    }

    T operator ++ () {
        ++i; return static_cast<T&>(*this);
    }

    T& operator -- (int) {
        --i; return static_cast<T&>(*this);
    }

    T operator -- () {
        --i; return static_cast<T&>(*this);
    }

    template<typename U>
    T operator + (U d) const {
        return i + d;
    }

    template<typename U, typename V>
    auto operator - (const ptr_iterator_base_impl<U,V>& iter) const -> decltype(i - iter.i) {
        return i - iter.i;
    }

    template<typename U, typename enable = typename std::enable_if<std::is_arithmetic<U>::value>::type>
    T operator - (U d) const {
        return i - d;
    }

    template<typename U, typename enable = typename std::enable_if<std::is_arithmetic<U>::value>::type>
    T& operator += (U d) {
        i += d; return static_cast<T&>(*this);
    }

    template<typename U, typename enable = typename std::enable_if<std::is_arithmetic<U>::value>::type>
    T& operator -= (U d) {
        i -= d; return static_cast<T&>(*this);
    }

    template<typename U, typename V>
    bool operator == (const ptr_iterator_base_impl<U,V>& iter) const {
        return i == iter.i;
    }

    template<typename U, typename V>
    bool operator != (const ptr_iterator_base_impl<U,V>& iter) const {
        return i != iter.i;
    }

    template<typename U, typename V>
    bool operator < (const ptr_iterator_base_impl<U,V>& iter) const {
        return i < iter.i;
    }

    template<typename U, typename V>
    bool operator <= (const ptr_iterator_base_impl<U,V>& iter) const {
        return i <= iter.i;
    }

    template<typename U, typename V>
    bool operator > (const ptr_iterator_base_impl<U,V>& iter) const {
        return i > iter.i;
    }

    template<typename U, typename V>
    bool operator >= (const ptr_iterator_base_impl<U,V>& iter) const {
        return i >= iter.i;
    }

    auto operator * () const -> decltype(**i) {
        return **i;
    }

    auto operator -> () const -> decltype(*i) {
        return *i;
    }

    I stdbase() const {
        return i;
    }
};

template<typename T, typename C>
class ptr_iterator_base;

template<typename T, typename C>
class const_ptr_iterator_base : public ptr_iterator_base_impl<const_ptr_iterator_base<T,C>, T> {
    friend C;
    template<typename N, typename K>
    friend class const_reverse_ptr_iterator_base;
    using impl = ptr_iterator_base_impl<const_ptr_iterator_base, T>;
    friend impl;

    const_ptr_iterator_base(const T& j) : impl(j) {}

public :
    const_ptr_iterator_base() = default;
    const_ptr_iterator_base(const ptr_iterator_base<T,C>& iter) : impl(iter.i) {}
    const_ptr_iterator_base(const const_ptr_iterator_base&) = default;
    const_ptr_iterator_base(const_ptr_iterator_base&&) = default;
    const_ptr_iterator_base& operator = (const const_ptr_iterator_base&) = default;
    const_ptr_iterator_base& operator = (const_ptr_iterator_base&&) = default;

    using impl::operator*;
    using impl::operator->;
    using impl::operator+;
    using impl::operator-;
    using impl::operator++;
    using impl::operator--;
    using impl::operator+=;
    using impl::operator-=;
    using impl::operator==;
    using impl::operator!=;
    using impl::operator<;
    using impl::operator<=;
    using impl::operator>;
    using impl::operator>=;
};


template<typename T, typename C>
class ptr_iterator_base : public ptr_iterator_base_impl<ptr_iterator_base<T,C>, T> {
    friend C;
    template<typename N, typename K>
    friend class const_ptr_iterator_base;
    template<typename N, typename K>
    friend class reverse_ptr_iterator_base;
    using impl = ptr_iterator_base_impl<ptr_iterator_base, T>;
    friend impl;

    ptr_iterator_base(const T& j) : impl(j) {}

public :
    ptr_iterator_base() = default;
    ptr_iterator_base(const ptr_iterator_base&) = default;
    ptr_iterator_base(ptr_iterator_base&&) = default;
    ptr_iterator_base& operator = (const ptr_iterator_base&) = default;
    ptr_iterator_base& operator = (ptr_iterator_base&&) = default;

    using impl::operator*;
    using impl::operator->;
    using impl::operator+;
    using impl::operator-;
    using impl::operator++;
    using impl::operator--;
    using impl::operator+=;
    using impl::operator-=;
    using impl::operator==;
    using impl::operator!=;
    using impl::operator<;
    using impl::operator<=;
    using impl::operator>;
    using impl::operator>=;
};

template<typename T, typename C>
class reverse_ptr_iterator_base;

template<typename T, typename C>
class const_reverse_ptr_iterator_base : public ptr_iterator_base_impl<const_reverse_ptr_iterator_base<T,C>, T> {
    friend C;
    using impl = ptr_iterator_base_impl<const_reverse_ptr_iterator_base, T>;
    friend impl;

    const_reverse_ptr_iterator_base(const T& j) : impl(j) {}

public :
    const_reverse_ptr_iterator_base() = default;
    const_reverse_ptr_iterator_base(const reverse_ptr_iterator_base<T,C>& iter) : impl(iter.i) {}
    const_reverse_ptr_iterator_base(const const_reverse_ptr_iterator_base&) = default;
    const_reverse_ptr_iterator_base(const_reverse_ptr_iterator_base&&) = default;
    const_reverse_ptr_iterator_base& operator = (const const_reverse_ptr_iterator_base&) = default;
    const_reverse_ptr_iterator_base& operator = (const_reverse_ptr_iterator_base&&) = default;

    using impl::operator*;
    using impl::operator->;
    using impl::operator+;
    using impl::operator-;
    using impl::operator++;
    using impl::operator--;
    using impl::operator+=;
    using impl::operator-=;
    using impl::operator==;
    using impl::operator!=;
    using impl::operator<;
    using impl::operator<=;
    using impl::operator>;
    using impl::operator>=;

    const_ptr_iterator_base<T,C> base() const {
        return this->i.base();
    }
};


template<typename T, typename C>
class reverse_ptr_iterator_base : public ptr_iterator_base_impl<reverse_ptr_iterator_base<T,C>, T> {
    friend C;
    template<typename N, typename K>
    friend class const_reverse_ptr_iterator_base;
    using impl = ptr_iterator_base_impl<reverse_ptr_iterator_base, T>;
    friend impl;

    reverse_ptr_iterator_base(const T& j) : impl(j) {}

public :
    reverse_ptr_iterator_base() = default;
    reverse_ptr_iterator_base(const reverse_ptr_iterator_base&) = default;
    reverse_ptr_iterator_base(reverse_ptr_iterator_base&&) = default;
    reverse_ptr_iterator_base& operator = (const reverse_ptr_iterator_base&) = default;
    reverse_ptr_iterator_base& operator = (reverse_ptr_iterator_base&&) = default;

    using impl::operator*;
    using impl::operator->;
    using impl::operator+;
    using impl::operator-;
    using impl::operator++;
    using impl::operator--;
    using impl::operator+=;
    using impl::operator-=;
    using impl::operator==;
    using impl::operator!=;
    using impl::operator<;
    using impl::operator<=;
    using impl::operator>;
    using impl::operator>=;

    ptr_iterator_base<T,C> base() const {
        return this->i.base();
    }
};

template<typename T>
struct ptr_to_raw;

template<typename T>
struct ptr_to_raw<T*> {
    using type = T;
};

template<typename T>
struct ptr_to_raw<std::unique_ptr<T>> {
    using type = T;
};

namespace std {
    template<typename T, typename C>
    struct iterator_traits<ptr_iterator_base<T,C>> {
        using std_iterator = T;
        using raw_value_type = typename iterator_traits<T>::value_type;
        using difference_type = typename iterator_traits<T>::difference_type;
        using value_type = typename ptr_to_raw<typename iterator_traits<T>::value_type>::type;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = typename iterator_traits<T>::iterator_category;
    };

    template<typename T, typename C>
    struct iterator_traits<const_ptr_iterator_base<T,C>> {
        using std_iterator = T;
        using raw_value_type = typename iterator_traits<T>::value_type;
        using difference_type = typename iterator_traits<T>::difference_type;
        using value_type = typename ptr_to_raw<typename iterator_traits<T>::value_type>::type;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = typename iterator_traits<T>::iterator_category;
    };

    template<typename T, typename C>
    struct iterator_traits<reverse_ptr_iterator_base<T,C>> {
        using std_iterator = T;
        using raw_value_type = typename iterator_traits<T>::value_type;
        using difference_type = typename iterator_traits<T>::difference_type;
        using value_type = typename ptr_to_raw<typename iterator_traits<T>::value_type>::type;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = typename iterator_traits<T>::iterator_category;
    };

    template<typename T, typename C>
    struct iterator_traits<const_reverse_ptr_iterator_base<T,C>> {
        using std_iterator = T;
        using raw_value_type = typename iterator_traits<T>::value_type;
        using difference_type = typename iterator_traits<T>::difference_type;
        using value_type = typename ptr_to_raw<typename iterator_traits<T>::value_type>::type;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = typename iterator_traits<T>::iterator_category;
    };

    template<typename I, typename C, typename F>
    void sort(const ptr_iterator_base<I,C>& i1, const ptr_iterator_base<I,C>& i2, F func) {
        using vtype = typename iterator_traits<I>::value_type;
        return std::sort(i1.stdbase(), i2.stdbase(), [&func](const vtype& v1, const vtype& v2) {
            return func(*v1, *v2);
        });
    }
}

#endif
