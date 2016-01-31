#ifndef CREDENTIAL_HPP
#define CREDENTIAL_HPP

#include <string>
#include <array>
#include <sorted_vector.hpp>
#include <delegate.hpp>
#include <variadic.hpp>
#include <serialized_packet.hpp>

namespace impl {
    template<typename T>
    struct requires_credentials_t {
        template <typename U> static std::true_type  dummy(typename std::decay<
            decltype(U::credentials)>::type*);
        template <typename U> static std::false_type dummy(...);
        using type = decltype(dummy<T>(0));
    };
}

template<typename T>
using requires_credentials = typename impl::requires_credentials_t<T>::type;

// Compile time list of credentials
using constant_credential_t = const char*;

template<typename... Args>
constexpr std::array<constant_credential_t, sizeof...(Args)> make_credential_array(Args&&... args) {
     return std::array<constant_credential_t, sizeof...(Args)>{{ std::forward<Args>(args)... }};
}

#define NETCOM_REQUIRES(...) \
    static constexpr auto credentials = make_credential_array(__VA_ARGS__)

// Trick to avoid undefined static member template errors in debug mode
template<typename T>
constant_credential_t get_credential(std::size_t i) {
    static constexpr auto credentials = T::credentials;
    return credentials[i];
}

// Trick to avoid undefined static member template errors in debug mode
template<typename T>
std::size_t get_num_credentials() {
    static constexpr std::size_t num = T::credentials.size();
    return num;
}

class constant_credential_list_t {
    ctl::delegate<constant_credential_t(std::size_t)> provider_;
    std::size_t len_;

public :
    template<typename T>
    constexpr constant_credential_list_t(ctl::type_list<T>) :
        provider_([](std::size_t i) { return get_credential<T>(i); }),
        len_(get_num_credentials<T>()) {}

    class iterator {
        const constant_credential_list_t* parent_ = nullptr;
        std::size_t i_ = 0;

        friend class constant_credential_list_t;
        iterator(const constant_credential_list_t* p, std::size_t i) : parent_(p), i_(i) {}

    public :
        iterator() = default;

        iterator& operator ++ () {
            ++i_;
            return *this;
        }

        constant_credential_t operator * () {
            return parent_->provider_(i_);
        }

        bool operator != (const iterator& iter) const {
            return parent_ != iter.parent_ || i_ != iter.i_;
        }
    };

    iterator begin() const {
        return iterator(this, 0);
    }

    iterator end() const {
        return iterator(this, len_);
    }
};


/// Type of a credential, needed to issue some requests.
using credential_t = std::string;

/// Runtime list of credentials.
class credential_list_t {
    ctl::sorted_vector<credential_t> list_;

    bool implies_(const credential_t& c1, const credential_t& c2) const;

public :
    credential_list_t() = default;
    credential_list_t(const credential_list_t&) = default;
    credential_list_t(credential_list_t&&) = default;
    credential_list_t(std::initializer_list<credential_t> lst);

    credential_list_t& operator=(const credential_list_t&) = default;
    credential_list_t& operator=(credential_list_t&&) = default;

    void grant(const credential_t& granted);
    void grant(const credential_list_t& granted);
    void remove(const credential_t& removed);
    void remove(const credential_list_t& removed);
    void clear();
    bool empty() const;

    using const_iterator = ctl::sorted_vector<credential_t>::const_iterator;
    using iterator = const_iterator;

    const_iterator begin() const;
    const_iterator end() const;

    friend packet_t::base& operator << (packet_t::base& t, const credential_list_t& l);
    friend packet_t::base& operator >> (packet_t::base& t, credential_list_t& l);
};

#endif
