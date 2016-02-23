#ifndef ITERATOR_BASE_HPP
#define ITERATOR_BASE_HPP

#include <iterator>

namespace ctl {
    template<typename ChildIter, typename BaseIter>
    struct ptr_iterator_base_impl {
        BaseIter i;

        ptr_iterator_base_impl(const BaseIter& j) : i(j) {}

        ChildIter operator ++ (int) {
            ChildIter iter = static_cast<ChildIter&>(*this);
            ++i; return iter;
        }

        ChildIter& operator ++ () {
            ++i; return static_cast<ChildIter&>(*this);
        }

        ChildIter operator -- (int) {
            ChildIter iter = static_cast<ChildIter&>(*this);
            --i; return iter;
        }

        ChildIter& operator -- () {
            --i; return static_cast<ChildIter&>(*this);
        }

        template<typename U>
        ChildIter operator + (U d) const {
            return i + d;
        }

        auto operator - (const ChildIter& iter) const -> decltype(i - i) {
            return i - iter.i;
        }

        template<typename U, typename enable = typename std::enable_if<std::is_arithmetic<U>::value>::type>
        ChildIter operator - (U d) const {
            return i - d;
        }

        template<typename U, typename enable = typename std::enable_if<std::is_arithmetic<U>::value>::type>
        ChildIter& operator += (U d) {
            i += d; return static_cast<ChildIter&>(*this);
        }

        template<typename U, typename enable = typename std::enable_if<std::is_arithmetic<U>::value>::type>
        ChildIter& operator -= (U d) {
            i -= d; return static_cast<ChildIter&>(*this);
        }

        bool operator == (const ChildIter& iter) const {
            return i == iter.i;
        }

        bool operator != (const ChildIter& iter) const {
            return i != iter.i;
        }

        bool operator < (const ChildIter& iter) const {
            return i < iter.i;
        }

        bool operator <= (const ChildIter& iter) const {
            return i <= iter.i;
        }

        bool operator > (const ChildIter& iter) const {
            return i > iter.i;
        }

        bool operator >= (const ChildIter& iter) const {
            return i >= iter.i;
        }

        auto operator * () const -> decltype(**i) {
            return **i;
        }

        auto operator -> () const -> decltype(*i) {
            return *i;
        }

        BaseIter stdbase() const {
            return i;
        }
    };

    template<typename BaseIter>
    class ptr_iterator_base;

    template<typename BaseIter>
    class const_ptr_iterator_base :
        private ptr_iterator_base_impl<const_ptr_iterator_base<BaseIter>, BaseIter> {

        template<typename N>
        friend class const_reverse_ptr_iterator_base;

        using impl = ptr_iterator_base_impl<const_ptr_iterator_base, BaseIter>;
        friend impl;

    public :
        const_ptr_iterator_base() = default;
        const_ptr_iterator_base(const BaseIter& j) : impl(j) {}
        const_ptr_iterator_base(const ptr_iterator_base<BaseIter>& iter) : impl(iter.i) {}
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
        using impl::stdbase;
    };

    template<typename BaseIter>
    class ptr_iterator_base :
        private ptr_iterator_base_impl<ptr_iterator_base<BaseIter>, BaseIter> {

        template<typename N>
        friend class const_ptr_iterator_base;
        template<typename N>
        friend class reverse_ptr_iterator_base;

        using impl = ptr_iterator_base_impl<ptr_iterator_base, BaseIter>;
        friend impl;

    public :
        ptr_iterator_base() = default;
        ptr_iterator_base(const BaseIter& j) : impl(j) {}
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
        using impl::stdbase;
    };

    template<typename BaseIter>
    class reverse_ptr_iterator_base;

    template<typename BaseIter>
    class const_reverse_ptr_iterator_base :
        private ptr_iterator_base_impl<const_reverse_ptr_iterator_base<BaseIter>, BaseIter> {

        using impl = ptr_iterator_base_impl<const_reverse_ptr_iterator_base, BaseIter>;
        friend impl;

    public :
        const_reverse_ptr_iterator_base() = default;
        const_reverse_ptr_iterator_base(const BaseIter& j) : impl(j) {}
        const_reverse_ptr_iterator_base(const reverse_ptr_iterator_base<BaseIter>& iter) : impl(iter.i) {}
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
        using impl::stdbase;

        const_ptr_iterator_base<BaseIter> base() const {
            return this->i.base();
        }
    };


    template<typename BaseIter>
    class reverse_ptr_iterator_base :
        private ptr_iterator_base_impl<reverse_ptr_iterator_base<BaseIter>, BaseIter> {

        template<typename N>
        friend class const_reverse_ptr_iterator_base;

        using impl = ptr_iterator_base_impl<reverse_ptr_iterator_base, BaseIter>;
        friend impl;

    public :
        reverse_ptr_iterator_base() = default;
        reverse_ptr_iterator_base(const BaseIter& j) : impl(j) {}
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
        using impl::stdbase;

        ptr_iterator_base<BaseIter> base() const {
            return this->i.base();
        }
    };

    template<typename T>
    struct ptr_to_raw;

    template<typename T>
    struct ptr_to_raw<T*> {
        using type = T;
    };

    template<typename T, typename D>
    struct ptr_to_raw<std::unique_ptr<T,D>> {
        using type = T;
    };
}

namespace std {
    template<typename T>
    struct iterator_traits<ctl::ptr_iterator_base<T>> {
        using std_iterator = T;
        using raw_value_type = typename iterator_traits<T>::value_type;
        using difference_type = typename iterator_traits<T>::difference_type;
        using value_type = typename ctl::ptr_to_raw<typename iterator_traits<T>::value_type>::type;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = typename iterator_traits<T>::iterator_category;
    };

    template<typename T>
    struct iterator_traits<ctl::const_ptr_iterator_base<T>> {
        using std_iterator = T;
        using raw_value_type = typename iterator_traits<T>::value_type;
        using difference_type = typename iterator_traits<T>::difference_type;
        using value_type = typename ctl::ptr_to_raw<typename iterator_traits<T>::value_type>::type;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = typename iterator_traits<T>::iterator_category;
    };

    template<typename T>
    struct iterator_traits<ctl::reverse_ptr_iterator_base<T>> {
        using std_iterator = T;
        using raw_value_type = typename iterator_traits<T>::value_type;
        using difference_type = typename iterator_traits<T>::difference_type;
        using value_type = typename ctl::ptr_to_raw<typename iterator_traits<T>::value_type>::type;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = typename iterator_traits<T>::iterator_category;
    };

    template<typename T>
    struct iterator_traits<ctl::const_reverse_ptr_iterator_base<T>> {
        using std_iterator = T;
        using raw_value_type = typename iterator_traits<T>::value_type;
        using difference_type = typename iterator_traits<T>::difference_type;
        using value_type = typename ctl::ptr_to_raw<typename iterator_traits<T>::value_type>::type;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = typename iterator_traits<T>::iterator_category;
    };

    template<typename I, typename F>
    void sort(const ctl::ptr_iterator_base<I>& i1, const ctl::ptr_iterator_base<I>& i2, F func) {
        using vtype = typename iterator_traits<I>::value_type;
        return std::sort(i1.stdbase(), i2.stdbase(), [&func](const vtype& v1, const vtype& v2) {
            return func(*v1, *v2);
        });
    }
}

#endif
