#ifndef SCOPED_HPP
#define SCOPED_HPP

/// Function that is automatically called once on destruction.
/** This simple helper is useful to make exception safe code, mostly when working with external C
    library, but not only. Example:

    {
        data_t* data = nullptr;
        some_c_lib_init_data(&data);

        // C++ code that can throw
        // ...

        some_c_lib_free_data(&data);
    }

    This code can be made exception safe by wrapping the "free" call inside a scoped_delegate:

    {
        data_t* data = nullptr;
        some_c_lib_init_data(&data);
        auto s = make_scoped([&]() {
            some_c_lib_free_data(&data);
        });

        // C++ code that can throw
        // ...
    }
**/
template<typename T>
struct scoped_t {
    ~scoped_t() {
        if (!done_) {
            do_();
        }
    }

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


template<typename U>
scoped_t<U> make_scoped(U&& u) {
    return scoped_t<U>(std::forward<U>(u));
}

#endif
