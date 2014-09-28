#ifndef PTR_VECTOR_HPP
#define PTR_VECTOR_HPP

#include <vector>
#include <algorithm>
#include <functional>
#include <memory>
#include "ptr_iterator_base.hpp"

namespace ctl {
    template<typename T>
    class ptr_vector : private std::vector<std::unique_ptr<T>> {
        using base = std::vector<std::unique_ptr<T>>;

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
        */
        ptr_vector() = default;

        /// Copy another vector into this one.
        ptr_vector(const ptr_vector& s) = default;

        /// Move another ptr_vector into this one.
        /** The other vector is left empty in the process, and all
            its content is transfered into this new one.
        **/
        ptr_vector(ptr_vector&& s) = default;

        /// Copy another ptr_vector into this one.
        ptr_vector& operator = (const ptr_vector& s) = default;

        /// Move another vector into this one.
        /** The other vector is left empty in the process, and all
            its content is transfered into this new one.
        **/
        ptr_vector& operator = (ptr_vector&& s) = default;

        /// Copy a pre-built vector into this one.
        explicit ptr_vector(const base& s) : base(s) {}

        /// Move a pre-built vector into this one.
        /** The provided vector is left empty in the process, and
            all its content is transfered into this new ptr_vector.
        **/
        explicit ptr_vector(base&& s) : base(std::move(s)) {}

        /// Insert the provided object in the vector.
        template<typename U=T, typename ... Args>
        iterator insert(iterator iter, Args&& ... args) {
            return base::insert(iter.stdbase(), std::unique_ptr<T>(new U(std::forward<Args>(args)...)));
        }

        /// Insert the provided object in the vector.
        template<typename U=T, typename ... Args>
        void emplace_back(Args&& ... args) {
            base::emplace_back(std::unique_ptr<T>(new U(std::forward<Args>(args)...)));
        }

        /// Insert the provided object in the vector.
        void push_back(T&& t) {
            emplace_back(std::move(t));
        }

        /// Erase an element from this vector.
        void erase(iterator iter) {
            base::erase(iter.stdbase());
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

        template<typename F>
        iterator find_if(F&& func) {
            return std::find_if(begin(), end(), std::forward<F>(func));
        }

        template<typename F>
        const_iterator find_if(F&& func) const {
            return std::find_if(begin(), end(), std::forward<F>(func));
        }

        template<typename F>
        iterator sort(F&& func) {
            return std::sort(begin(), end(), std::forward<F>(func));
        }

        template<typename F>
        const_iterator sort(F&& func) const {
            return std::sort(begin(), end(), std::forward<F>(func));
        }

        using base::size;
        using base::max_size;
        using base::capacity;
        using base::shrink_to_fit;
        using base::resize;
        using base::reserve;
        using base::empty;
        using base::clear;
        using base::push_back;
        using base::pop_back;

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
}

#endif
