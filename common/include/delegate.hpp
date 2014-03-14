#ifndef DELEGATE_HPP
#define DELEGATE_HPP

#include <type_traits>

namespace ctl {
    template<typename T>
    struct delegate;

    /// Wrapper to store lambda functions.
    /** This structure is a simple wrapper that, much like std::bind(), stores the "this" pointer of
        member functions in order to use member functions like normal functions.
        The bound member function does not need to have the same signature as that of the delegate. The
        delegate parameters just need to be implicitly convertible to that of the member function,
        and the converse is true for the return value.

        Use case:

        \code{.cpp}
            std::vector<delegate<int(int)>> v;
            v.push_back([](int i) { return 1*i; });
            v.push_back([](int i) { return 2*i; });
            v.push_back([](int i) { return 3*i; });
            for (auto& d : v) {
                std::cout << d(12) << std::endl;
            }
        \endcode

        It shares similar features with std::function, except that it does not support free functions
        and custom allocators (not needed). On the other hand, it is tailored for use with lambda
        functions (or any other functor), and provides substantial performance improvements over
        std::function (about a factor 2 faster with clang & gcc in -O3).

        Based on "Impossibly fast C++ delegates".
        http://www.codeproject.com/Articles/11015/The-Impossibly-Fast-C-Delegates
    **/
    template<typename R, typename ... Args>
    struct delegate<R(Args...)> {
        /// Construct a new delegate.
        /** In all constructors, the first argument is the object on which the delegate acts, and the
            second argument is the member function that is to be called. When the object is given as a
            reference or a pointer, then only a pointer to this object is kept inside the delegate, and
            the object is expected to outlive the delegate. On the other hand, is the object is given as
            an r-value (either a temporary or an r-value reference), a new instance is move constructed
            inside the delegate, and its lifetime is bound to that of the delegate.
            The member function can be given either as a function pointer, or as an
            std::integral_constant. The latter is usually faster because the function pointer is a
            template argument, and can be more easily inlined by the compiler, but it is also more
            tedious to write.
            Lastly, if no member function is provided, T::operator() is assumed.
        **/
        template<typename ... TArgs>
        delegate(TArgs&&... args) {
            create_(std::forward<TArgs>(args)...);
        }

        /// Default constructor.
        /** When default constructed, a delegate is in "empty" state. See empty().
        **/
        delegate() = default;
        delegate(std::nullptr_t) : delegate() {}

        ~delegate() {
            if (del_) {
                (*del_)(obj_);
            }
        }

        delegate(const delegate&) = delete;
        delegate& operator= (const delegate&) = delete;

        /// Move constructor.
        /** The delegate object that is moved into this one is left in empty state.
        **/
        delegate(delegate&& d) :
            fun_(d.fun_), wfun_(d.wfun_), del_(d.del_), mov_(d.mov_) {
            if (mov_) {
                (*mov_)(std::move(d), *this);
                d.mov_ = nullptr;
            } else {
                obj_ = d.obj_;
            }

            d.obj_ = nullptr;
            d.del_ = nullptr;
            d.fun_ = nullptr;
            d.wfun_ = nullptr;
        }

        /// Move assignment.
        /** The delegate object that is moved into this one is left in empty state.
        **/
        delegate& operator= (delegate&& d) {
            if (del_) {
                (*del_)(obj_);
            }

            del_ = d.del_;
            mov_ = d.mov_;
            fun_ = d.fun_;
            wfun_ = d.wfun_;

            if (mov_) {
                (*mov_)(std::move(d), *this);
                d.mov_ = nullptr;
            } else {
                obj_ = d.obj_;
            }

            d.obj_ = nullptr;
            d.del_ = nullptr;
            d.fun_ = nullptr;
            d.wfun_ = nullptr;

            return *this;
        }

        /// Put the delegate in "empty" state. See empty().
        void clear() {
            if (del_) {
                (*del_)(obj_);
            }

            obj_ = nullptr;
            del_ = nullptr;
            mov_ = nullptr;
            fun_ = nullptr;
            wfun_ = nullptr;
        }

        /// Re-assign the underlying object and function.
        template<typename ... TArgs>
        void assign(TArgs&&... args) {
            clear();
            create_(std::forward<TArgs>(args)...);
        }

        /// Return 'true' if in "empty" state.
        /** When in "empty" state, a delegate has minimal memory usage.
            Trying to call an empty delegate will crash the program.
        **/
        bool empty() const {
            return obj_ == nullptr;
        }

        /// Check if the delegate is not in "empty" state.
        explicit operator bool () const {
            return !empty();
        }

        bool operator == (std::nullptr_t) const {
            return empty();
        }

        bool operator != (std::nullptr_t) const {
            return !empty();
        }

        /// Call the delegate.
        template<typename ... TArgs>
        R operator() (TArgs&& ... args) const {
            return (*wfun_)(obj_, fun_, std::forward<TArgs>(args)...);
        }

    private :
        using wfun_t = R (*)(void*, void*, Args...);
        using dfun_t = void (*)(void*);
        using mfun_t = void (*)(delegate&&, delegate&);

        static const std::size_t max_small_object_size = 2*sizeof(std::size_t);

        void* obj_ = nullptr;
        char inplace_[max_small_object_size];

        void* fun_ = nullptr;
        wfun_t wfun_ = nullptr;

        dfun_t del_ = nullptr;
        mfun_t mov_ = nullptr;

        template<typename T>
        static void deleter_(void* obj) {
            delete static_cast<T*>(obj);
        }

        template<typename T>
        static void inplace_deleter_(void* obj) {
            static_cast<T*>(obj)->~T();
        }

        template<typename T>
        static void inplace_mover_(delegate&& from, delegate& to) {
            to.obj_ = new ((void*)to.inplace_) T(std::move(*static_cast<T*>(from.obj_)));
            inplace_deleter_<T>(from.obj_);
        }

        template<typename T, typename M>
        static R wrapper_(void* obj, void* fun, Args... args) {
            M cfun = nullptr;
            *reinterpret_cast<void**>(&cfun) = fun;
            return (static_cast<T*>(obj)->*cfun)(std::forward<Args>(args)...);
        }

        template<typename T, typename M, M m>
        static R template_wrapper_(void* obj, void*, Args... args) {
            return (static_cast<T*>(obj)->*m)(std::forward<Args>(args)...);
        }

        template<typename T>
        void init_obj_(T& obj) {
            obj_ = &obj;
        }

        template<typename T>
        void init_obj_(T* obj) {
            obj_ = obj;
        }

        template<typename T>
        void init_obj_(T&& obj, std::false_type) {
            obj_ = new T(std::forward<T>(obj));
            del_ = &deleter_<T>;
        }

        template<typename T>
        void init_obj_(T&& obj, std::true_type) {
            // Small object optimization
            obj_ = new ((void*)inplace_) T(std::forward<T>(obj));
            del_ = &inplace_deleter_<T>;
            mov_ = &inplace_mover_<T>;
        }

        template<typename T>
        void init_obj_(T&& obj) {
            init_obj_(std::forward<T>(obj),
                std::integral_constant<bool, sizeof(T) <= max_small_object_size>{});
        }

        template<typename T, typename FT, FT F>
        void init_fun_(std::integral_constant<FT,F>) {
            using RT = typename std::remove_reference<T>::type;
            wfun_ = &template_wrapper_<RT, FT, F>;
        }

        template<typename T, typename M>
        void init_fun_(M fun) {
            fun_ = *reinterpret_cast<void**>(&fun);
            using RT = typename std::remove_reference<T>::type;
            wfun_ = &wrapper_<RT, M>;
        }

        template<typename T, typename M>
        void create_(T&& obj, M&& fun) {
            init_obj_(std::forward<T>(obj));
            init_fun_<T>(std::forward<M>(fun));
        }

        template<typename T>
        void create_(T&& obj) {
            init_obj_(std::forward<T>(obj));
            using RT = typename std::remove_reference<T>::type;
            init_fun_<T>(std::integral_constant<decltype(&RT::operator()),&RT::operator()>());
        }
    };
}

#define make_delegate(obj,func) ctl::delegate<ctl::get_signature<decltype(func)>>( \
    obj, std::integral_constant<decltype(func),func>{})

#endif
