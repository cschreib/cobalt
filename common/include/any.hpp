#ifndef ANY_HPP
#define ANY_HPP

#include <array>
#include <vector>
#include "packet.hpp"

enum class any_type : std::uint8_t {
    none = 0,
    boolean,
    int8, int16, int32,
    uint8, uint16, uint32,
    float32, float64,
    string
};

namespace any_impl {
    template<typename T>
    struct any_traits;

    template<typename T>
    struct any_traits_single {
        static const std::uint32_t size = 1u;
        using etype = T;
        static void extract(T& t, const T* data, std::uint32_t size) {
            t = data;
        }
        static void insert(T&& t, void*& data, std::uint32_t& size) {
            size = 1u;
            data = new T(std::forward<T>(t));
        }
    };

    template<> struct any_traits<bool> : any_traits_single<bool> {
        static const any_type type = any_type::boolean;
    };
    template<> struct any_traits<std::int8_t> : any_traits_single<std::int8_t> {
        static const any_type type = any_type::int8;
    };
    template<> struct any_traits<std::int16_t> : any_traits_single<std::int16_t> {
        static const any_type type = any_type::int16;
    };
    template<> struct any_traits<std::int32_t> : any_traits_single<std::int32_t> {
        static const any_type type = any_type::int32;
    };
    template<> struct any_traits<std::uint8_t> : any_traits_single<std::uint8_t> {
        static const any_type type = any_type::uint8;
    };
    template<> struct any_traits<std::uint16_t> : any_traits_single<std::uint16_t> {
        static const any_type type = any_type::uint16;
    };
    template<> struct any_traits<std::uint32_t> : any_traits_single<std::uint32_t> {
        static const any_type type = any_type::uint32;
    };
    template<> struct any_traits<float> : any_traits_single<float> {
        static const any_type type = any_type::float32;
    };
    template<> struct any_traits<double> : any_traits_single<double> {
        static const any_type type = any_type::float64;
    };
    template<> struct any_traits<std::string> : any_traits_single<std::string> {
        static const any_type type = any_type::string;
    };

    template<typename T, typename A> struct any_traits<std::vector<T,A>> {
        static const any_type type = any_traits<T>::type;
        static const std::uint32_t size = 0u;
        using etype = T;
        static void extract(std::vector<T,A>& t, const T* data, std::uint32_t size) {
            t.reserve(size);
            for (std::uint32_t i = 0; i < size; ++i) {
                t.push_back(data[i]);
            }
        }
        static void insert(const std::vector<T,A>& t, void*& data, std::uint32_t& size) {
            size = t.size();
            if (size != 0u) {
                T* cdata = new T[size];
                data = cdata;
                for (std::uint32_t i = 0; i < size; ++i) {
                    cdata[i] = t[i];
                }
            }
        }
    };
    template<typename T, std::size_t N> struct any_traits<std::array<T,N>> {
        static const any_type type = any_traits<T>::type;
        static const std::uint32_t size = N;
        using etype = T;
        static void extract(std::array<T,N>& t, const T* data, std::uint32_t size) {
            for (std::uint32_t i = 0; i < size; ++i) {
                t[i] = data[i];
            }
        }
        static void insert(const std::array<T,N>& t, void*& data, std::uint32_t& size) {
            size = t.size();
            if (size != 0u) {
                T* cdata = new T[size];
                data = cdata;
                for (std::uint32_t i = 0; i < size; ++i) {
                    cdata[i] = t[i];
                }
            }
        }
    };
}

struct any {
    any() = default;
    any(any&& a);

    template<typename T>
    any(T&& t) : type_(any_impl::any_traits<T>::type) {
        any_impl::any_traits<T>::insert(std::forward<T>(t), data_, size_);
    }

    ~any();

    any& operator = (any&& a);

    any_type type() const;
    std::uint32_t size() const;

    template<typename T>
    bool extract(T& t) const {
        if (type_ != any_impl::any_traits<T>::type) return false;
        if (any_impl::any_traits<T>::size != 0u && 
            size_ > any_impl::any_traits<T>::size) return false;

        using etype = typename any_impl::any_traits<T>::etype;
        const etype* cdata = reinterpret_cast<const etype*>(data_);
        any_impl::any_traits<T>::extract(t, cdata, size_);

        return true;
    }

private :
    friend sf::Packet& operator << (sf::Packet& p, const any& data);
    friend sf::Packet& operator >> (sf::Packet& p, any& data);

    any_type type_ = any_type::none;
    std::uint32_t size_ = 0u;
    void* data_ = nullptr;
};

#endif
