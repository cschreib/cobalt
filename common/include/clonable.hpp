#ifndef CLONABLE_HPP
#define CLONABLE_HPP

#include <type_traits>

/* clonable<T>
--------------------------------------------------
  This base abstract class allows one to give the 'clonable' concept to another class.

  T::clone() :
   - if a 'do_clone' method is defined in the body of T (it must take no argument and return a
     T*), then T::clone() amounts to call this method.
   - if T is copy constructible and no 'do_clone' method is defined, then T::clone() will create
     a new T and use the copy constructor to construct a perfect clone of the current object.

  Typical usage :
   - with private inheritance :
    (inheritance should be private because there
    is no real meaning in conveting a child* into
    a clonable<child>*)

  class child : private clonable<child> {
      friend class clonable<child>;

  public :

      using clonable<child>::clone;

      // If, for some reason, 'child' is not copyable or if one wants to provide a particular
      // clone function, one can define (in any scope, since 'clonable' is a friend of 'child')
      // the following function :
      child* do_clone() const {
          // ...
      }
  };

   - with public inheritance :

   class child : public clonable<child> {
   public :
      // If, for some reason, 'child' is not copyable or if one wants to provide a particular
      // clone function, one can define (only in public scope, since 'clonable' is a priori not
      // a friend of 'child') the following function :
      child* do_clone() const {
          // ...
      }
   };
*/

namespace clonable_impl {
    template<bool copy_constructible>
    struct copy;

    template<bool do_clone>
    struct clone;
}

template<typename child>
class clonable {
    template<bool b>
    friend class clonable_impl::copy;
    template<bool b>
    friend class clonable_impl::clone;

    child* use_copy_constructor_() const {
        return new child(static_cast<const child&>(*this));
    }

    child* use_clone_function_() const {
        return static_cast<const child&>(*this).do_clone();
    }

public :

    virtual ~clonable() {}

    struct has_do_clone {
        template <typename U> static std::true_type  dummy(decltype(((U*)0)->do_clone())*);
        template <typename U> static std::false_type dummy(...);
        static const bool value = decltype(dummy<child>(0))::value;
    };

    virtual child* clone() const;
};

namespace clonable_impl {
    template<>
    struct clone<true> {
        template<typename T>
        static T* do_clone(const clonable<T>& t) {
            return t.use_clone_function_();
        }
    };

    template<>
    struct clone<false> {
        template<typename T>
        static T* do_clone(const clonable<T>& t) {
            return t.use_copy_constructor_();
        }
    };

    template<>
    struct copy<true> {
        template<typename T>
        static T* do_clone(const clonable<T>& t) {
            return clone<clonable<T>::has_do_clone::value>::template do_clone<T>(t);
        }
    };

    template<>
    struct copy<false> {
        template<typename T>
        static T* do_clone(const clonable<T>& t) {
            return t.use_clone_function_();
        }
    };

    template<typename T>
    T* do_clone(const clonable<T>& t) {
        return copy<std::is_copy_constructible<T>::value>::template do_clone<T>(t);
    }
}

template<class child>
child* clonable<child>::clone() const {
    return clonable_impl::do_clone(*this);
}

/* is_clonable<T>
--------------------------------------------------
  This trait has a static boolean member 'value' that evaluates to 'true' if the type T has a
  member function 'clone()' that takes no argument and returns a T*. Else, 'value' evaluates to
  'false'.

  In particular, any class T that inherits from 'clonable<T>' and provides a valid 'clone'
  method falls in the former category.
*/
namespace clonable_impl {
    template<typename T>
    struct has_clone {
        template <typename U> static std::true_type  dummy(decltype(((U*)0)->clone())*);
        template <typename U> static std::false_type dummy(...);
        static const bool value = decltype(dummy<T>(0))::value;
    };

    template<typename T, bool clone, bool base_clonable>
    struct is_clonable;

    template<typename T, bool clone>
    struct is_clonable<T, clone, false> {
        static const bool value = false;
    };

    template<typename T>
    struct is_clonable<T, true, true> {
        static const bool value = std::is_same<decltype(((T*)0)->clone()), T*>::value;
    };

    template<typename T>
    struct is_clonable<T, false, true> {
        static const bool value = false;
    };
}

template<typename T>
using is_clonable = clonable_impl::is_clonable<T,
    clonable_impl::has_clone<T>::value,
    (std::is_base_of<clonable<T>, T>::value &&
    (std::is_copy_constructible<T>::value || clonable<T>::has_do_clone::value)) ||
    !std::is_base_of<clonable<T>, T>::value
>;

/* non_copyable
---------------------------------------------------
  When a class inherits from 'non_copyable', its copy constructor and assignment operator are
  automatically deleted, effectively making this class non copyable.
*/

struct non_copyable {
    non_copyable() = default;
    non_copyable(non_copyable&&) = default;
    non_copyable& operator = (non_copyable&&) = default;
    non_copyable(const non_copyable&) = delete;
    non_copyable& operator = (const non_copyable&) = delete;
};

/* non_movable
---------------------------------------------------
  When a class inherits from 'non_movable', both its copy/move constructor and assignment
  operator are automatically deleted, effectively making this class non copyable and non
  movable.
*/

struct non_movable {
    non_movable() = default;
    non_movable(non_movable&&) = delete;
    non_movable& operator = (non_movable&&) = delete;
    non_movable(const non_movable&) = delete;
    non_movable& operator = (const non_movable&) = delete;
};

#endif
