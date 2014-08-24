#ifndef SCOPED_HPP
#define SCOPED_HPP

namespace ctl {
    /// Function that is automatically called once on destruction.
    /** This simple helper is useful to make exception safe code, mostly when working with external C
        library, but not only. Example:

        \code{.cpp}
            {
                data_t* data = nullptr;
                some_c_lib_init_data(&data);

                // C++ code that can throw
                // ...

                some_c_lib_free_data(&data);
            }
        \endcode

        This code can be made exception safe by wrapping the "free" call inside a scoped_t:

        \code{.cpp}
            {
                data_t* data = nullptr;
                some_c_lib_init_data(&data);
                auto s = ctl::make_scoped([&]() {
                    some_c_lib_free_data(&data);
                });

                // C++ code that can throw
                // ...
            }
        \endcode
    **/
    template<typename T>
    struct scoped_t {
        scoped_t(const scoped_t&) = delete;
        scoped_t& operator= (const scoped_t&) = delete;
        scoped_t& operator= (scoped_t&& s) = delete;

        scoped_t(scoped_t&& s) : do_(std::move(s.do_)), done_(s.done_) {
            s.done_ = true;
        }

        /// Call the function.
        ~scoped_t() {
            if (!done_) {
                do_();
            }
        }

        /// Call the function now.
        /** The function will not be called in the destructor.
        **/
        void release() {
            do_();
            done_ = true;
        }

    private :
        template<typename U>
        explicit scoped_t(U&& u) : do_(std::forward<U>(u)) {}

        template<typename U>
        friend scoped_t<U> make_scoped(U&& u);

        T do_;
        bool done_ = false;
    };

    /// Build a new scoped_t from a given callback function.
    template<typename U>
    scoped_t<U> make_scoped(U&& u) {
        return scoped_t<U>(std::forward<U>(u));
    }

    namespace impl {
        template<typename T>
        struct assigner {
            T& obj;
            T val;

            template<typename U>
            assigner(T& o, U&& v) : obj(o), val(std::forward<U>(v)) {}

            void operator() () {
                obj = std::move(val);
            }
        };
    }

    /// Toggle the provided boolean and return a scoped_t that toggles it back.
    inline scoped_t<impl::assigner<bool>> scoped_toggle(bool& b) {
        b = !b;
        return make_scoped(impl::assigner<bool>(b, !b));
    }
}

#endif
