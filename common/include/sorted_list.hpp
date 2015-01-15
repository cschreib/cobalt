#ifndef SORTED_LIST_HPP
#define SORTED_LIST_HPP

#include <list>
#include <algorithm>
#include <functional>
#include "ptr_iterator_base.hpp"
#include "default_comparator.hpp"

namespace ctl {
    /// Sorted std::list wrapper.
    /** This class is a light alternative to std::set.
        Inspired from: [1] www.lafstern.org/matt/col1.pdf

        The sorted list achieves the same O(log(N)) look up complexity as std::set, but with a lower
        constant of proportionality thanks to the binary search algorithm (twice lower according to
        [1]).

        See sorted_vector_t for details.
    **/
    template<typename T, typename Cmp>
    class sorted_list_t : private std::list<T> {
        using base = std::list<T>;
        Cmp compare;

    public :
        using iterator = typename base::iterator;
        using const_iterator = typename base::const_iterator;
        using reverse_iterator = typename base::reverse_iterator;
        using const_reverse_iterator = typename base::const_reverse_iterator;

        /// Default constructor.
        /** Creates an empty vector.
        **/
        sorted_list_t() = default;

        /// Copy another vector into this one.
        sorted_list_t(const sorted_list_t& s) = default;

        /// Move another sorted_list into this one.
        /** The other vector is left empty in the process, and all its content is transfered into
            this new one.
        **/
        sorted_list_t(sorted_list_t&& s) = default;

        /// Copy another sorted_list into this one.
        sorted_list_t& operator = (const sorted_list_t& s) = default;

        /// Move another vector into this one.
        /** The other vector is left empty in the process, and all its content is transfered into
            this new one.
        **/
        sorted_list_t& operator = (sorted_list_t&& s) = default;

        /// Copy a pre-built vector into this one.
        /** The provided vector must be sorted, and shall not contain any duplicate value.
        **/
        explicit sorted_list_t(const base& s) : base(s) {}

        /// Move a pre-built vector into this one.
        /** The provided vector must be sorted, and shall not contain any duplicate value. It is
            left empty in the process, and all its content is transfered into this new sorted_list.
        **/
        explicit sorted_list_t(base&& s) : base(s) {}

        /// Default constructor, with comparator.
        /** Creates an empty vector and sets the comparator function.
        **/
        explicit sorted_list_t(const Cmp& c) : compare(c) {}

        /// Insert a copy of the provided object in the vector.
        /** If an object already exists with the same key, it is destroyed and replaced by this
            copy.
        **/
        iterator insert(const T& t) {
            return insert(T(t));
        }

        /// Insert the provided object in the vector.
        /** If an object already exists with the same key, it is destroyed and replaced by this one.
        **/
        iterator insert(T&& t) {
            if (empty()) {
                base::push_back(std::move(t));
                return base::begin();
            } else {
                auto iter = std::lower_bound(begin(), end(), t, compare);
                if (iter != end() && !compare(t, *iter)) {
                    *iter = std::move(t);
                } else {
                    iter = base::insert(iter, std::move(t));
                }
                return iter;
            }
        }

        /// Erase an element from this vector.
        void erase(iterator iter) {
            base::erase(iter);
        }

        /// Erase an element from this vector by its key.
        /** The key can be a copy of the element itself, or any other object that is supported by
            the chosen comparison function. If no object is found with that key, this function does
            nothing.
        **/
        template<typename Key>
        void erase(const Key& k) {
            auto iter = find(k);
            if (iter != end()) {
                base::erase(iter);
            }
        }

        /// Find an object in this vector by its key.
        /** The key can be a copy of the element itself, or any other object that is supported by
            the chosen comparison function. If no element is found, this function returns end().
        **/
        template<typename Key>
        iterator find(const Key& k) {
            if (empty()) {
                return end();
            } else {
                auto iter = std::lower_bound(begin(), end(), k, compare);
                if (iter != end() && !compare(k, *iter)) {
                    return iter;
                } else {
                    return end();
                }
            }
        }

        /// Find an object in this vector by its key.
        /** The key can be a copy of the element itself, or any other object that is supported by
            the chosen comparison function. If no element is found, this function returns end().
        **/
        template<typename Key>
        const_iterator find(const Key& k) const {
            if (empty()) {
                return end();
            } else {
                auto iter = std::lower_bound(begin(), end(), k, compare);
                if (iter != end() && !compare(k, *iter)) {
                    return iter;
                } else {
                    return end();
                }
            }
        }

        using base::front;
        using base::back;
        using base::pop_front;
        using base::pop_back;
        using base::size;
        using base::max_size;
        using base::empty;
        using base::clear;

        using base::begin;
        using base::rbegin;
        using base::end;
        using base::rend;
    };

    /// Sorted std::list wrapper, reference specialization.
    /** See sorted_list_t for details.
    **/
    template<typename T, typename Cmp>
    class sorted_list_t<T&, Cmp> : private std::vector<T*> {
        using base  = std::vector<T*>;
        Cmp compare;

        using base_iterator = typename base::iterator;
        using base_const_iterator = typename base::const_iterator;
        using base_reverse_iterator = typename base::reverse_iterator;
        using base_const_reverse_iterator = typename base::const_reverse_iterator;

    public :
        using iterator = ptr_iterator_base<base_iterator>;
        using const_iterator = const_ptr_iterator_base<base_const_iterator>;
        using reverse_iterator = reverse_ptr_iterator_base<base_reverse_iterator>;
        using const_reverse_iterator = const_reverse_ptr_iterator_base<base_const_reverse_iterator>;

        /// Default constructor.
        /** Creates an empty vector.
        **/
        sorted_list_t() = default;

        /// Copy another vector into this one.
        sorted_list_t(const sorted_list_t& s) = default;

        /// Move another sorted_list into this one.
        /** The other vector is left empty in the process, and all its content is transfered into
            this new one.
        **/
        sorted_list_t(sorted_list_t&& s) = default;

        /// Copy another sorted_list into this one.
        sorted_list_t& operator = (const sorted_list_t& s) = default;

        /// Move another vector into this one.
        /** The other vector is left empty in the process, and all its content is transfered into
            this new one.
        **/
        sorted_list_t& operator = (sorted_list_t&& s) = default;

        /// Copy a pre-built vector into this one.
        /** The provided vector must be sorted, and shall not contain any duplicate value.
        **/
        explicit sorted_list_t(const base& s) : base(s) {}

        /// Move a pre-built vector into this one.
        /** The provided vector must be sorted, and shall not contain any duplicate value. It is
            left empty in the process, and all its content is transfered into this new sorted_list.
        **/
        explicit sorted_list_t(base&& s) : base(s) {}

        /// Default constructor, with comparator.
        /** Creates an empty vector and sets the comparator function.
        **/
        explicit sorted_list_t(const Cmp& c) : compare(c) {}

        /// Insert the provided object in the vector.
        /** If an object already exists with the same key, it is destroyed and replaced by this one.
        **/
        iterator insert(T& t) {
            if (empty()) {
                base::push_back(&t);
                return base::begin();
            } else {
                auto iter = std::lower_bound(base::begin(), base::end(), &t, compare);
                if (iter != base::end() && !compare(&t, *iter)) {
                    *iter = &t;
                } else {
                    iter = base::insert(iter, &t);
                }
                return iter;
            }
        }

        /// Erase an element from this vector.
        void erase(iterator iter) {
            base::erase(iter.stdbase());
        }

        /// Erase an element from this vector by its key.
        /** The key can be a copy of the element itself, or any other object that is supported by
            the chosen comparison function. If no object is found with that key, this function does
            nothing.
        **/
        template<typename Key>
        void erase(const Key& k) {
            auto iter = find(k);
            if (iter != end()) {
                base::erase(iter.stdbase());
            }
        }

        /// Find an object in this vector by its key.
        /** The key can be a copy of the element itself, or any other object that is supported by
            the chosen comparison function. If no element is found, this function returns end().
        **/
        template<typename Key, typename enable = typename std::enable_if<!std::is_same<Key,T>::value>::type>
        iterator find(const Key& k) {
            if (empty()) {
                return end();
            } else {
                auto iter = std::lower_bound(begin(), end(), k, compare);
                if (iter != end() && !compare(k, *iter)) {
                    return iter;
                } else {
                    return end();
                }
            }
        }

        /// Find an object in this vector by its key.
        /** The key can be a copy of the element itself, or any other object that is supported by
            the chosen comparison function. If no element is found, this function returns end().
        **/
        iterator find(const T& t) {
            if (empty()) {
                return end();
            } else {
                auto iter = std::lower_bound(base::begin(), base::end(), &t, compare);
                if (iter != base::end() && !compare(&t, *iter)) {
                    return iter;
                } else {
                    return end();
                }
            }
        }

        /// Find an object in this vector by its key.
        /** The key can be a copy of the element itself, or any other object that is supported by
            the chosen comparison function. If no element is found, this function returns end().
        **/
        template<typename Key, typename enable = typename std::enable_if<!std::is_same<Key,T>::value>::type>
        const_iterator find(const Key& k) const {
            if (empty()) {
                return end();
            } else {
                auto iter = std::lower_bound(begin(), end(), k, compare);
                if (iter != end() && !compare(k, *iter)) {
                    return iter;
                } else {
                    return end();
                }
            }
        }

        /// Find an object in this vector by its key.
        /** The key can be a copy of the element itself, or any other object that is supported by
            the chosen comparison function. If no element is found, this function returns end().
        **/
        const_iterator find(const T& t) const {
            if (empty()) {
                return end();
            } else {
                auto iter = std::lower_bound(base::begin(), base::end(), &t, compare);
                if (iter != base::end() && !compare(&t, *iter)) {
                    return iter;
                } else {
                    return end();
                }
            }
        }

        /// Get the first object of this vector.
        T& front() {
            return *base::front();
        }

        /// Get the first object of this vector.
        const T& front() const {
            return *base::front();
        }

        /// Get the last object of this vector.
        T& back() {
            return *base::back();
        }

        /// Get the last object of this vector.
        const T& back() const {
            return *base::back();
        }

        using base::size;
        using base::max_size;
        using base::capacity;
        using base::reserve;
        using base::empty;
        using base::clear;

        iterator begin() {
            return base::begin();
        }

        const_iterator begin() const {
            return base::begin();
        }

        reverse_iterator rbegin() {
            return base::rbegin();
        }

        const_reverse_iterator rbegin() const {
            return base::rbegin();
        }

        iterator end() {
            return base::end();
        }

        const_iterator end() const {
            return base::end();
        }

        reverse_iterator rend() {
            return base::rend();
        }

        const_reverse_iterator rend() const {
            return base::rend();
        }
    };


    template<typename T, typename Cmp = typename default_comparator<T>::type>
    using sorted_list = sorted_list_t<T, Cmp>;
}

#endif
