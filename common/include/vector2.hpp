#ifndef VECTOR2_HPP
#define VECTOR2_HPP

#include <cmath>
#include <serialized_packet.hpp>

template<typename T>
struct vec2_t {
public :
    vec2_t() {}
    vec2_t(const vec2_t& v) = default;
    vec2_t& operator = (const vec2_t& v) = default;

    template<typename N>
    explicit vec2_t(const vec2_t<N>& v) : x(v.x), y(v.y) {}

    vec2_t(T mX, T mY) : x(mX), y(mY) {}

    void set(T mX, T mY) {
        x = mX; y = mY;
    }

    T get_norm() const {
        return sqrt(x*x + y*y);
    }

    T size() const {
        return get_norm();
    }

    T get_norm_squared() const {
        return x*x + y*y;
    }

    T size_squared() const {
        return get_norm_squared();
    }

    void normalize() {
        T norm = get_norm();
        x = x/norm;
        y = y/norm;
    }

    vec2_t get_unit() const {
        T norm = get_norm();
        return vec2_t(x/norm, y/norm);
    }

    template<typename N>
    void rotate(N angle) {
        static const N pi2 = 2.0*atan2(0.0, -1.0);
        vec2_t p;

        double ca = cos(angle*pi2), sa = sin(angle*pi2);

        p.x = x*ca - y*sa;
        p.y = x*sa + y*ca;

        x = p.x;
        y = p.y;
    }

    template<typename N>
    vec2_t get_rotated(N angle) const {
        static const N pi2 = 2.0*atan2(0.0, -1.0);
        double ca = cos(angle*pi2), sa = sin(angle*pi2);
        return vec2_t(x*ca - y*sa, x*sa + y*ca);
    }

    void scale(const vec2_t& v) {
        x *= v.x;
        y *= v.y;
    }

    vec2_t get_scale(const vec2_t& v) const {
        return vec2_t(x*v.x, y*v.y);
    }

    vec2_t operator + (const vec2_t& v) const {
        return vec2_t(x + v.x, y + v.y);
    }
    void operator += (const vec2_t& v) {
        x += v.x; y += v.y;
    }

    vec2_t operator - () const {
        return vec2_t(-x, -y);
    }

    vec2_t operator - (const vec2_t& v) const {
        return vec2_t(x - v.x, y - v.y);
    }
    void operator -= (const vec2_t& v) {
        x -= v.x; y -= v.y;
    }

    bool operator == (const vec2_t& v) const {
        return (x == v.x) && (y == v.y);
    }
    bool operator != (const vec2_t& v) const {
        return (x != v.x) || (y != v.y);
    }

    template<typename N>
    vec2_t operator * (N mValue) const {
        return vec2_t(x*mValue, y*mValue);
    }

    template<typename N>
    void operator *= (N mValue) {
        x *= mValue;  y *= mValue;
    }

    template<typename N>
    vec2_t operator / (N mValue) const {
        return vec2_t(x/mValue, y/mValue);
    }

    template<typename N>
    void operator /= (N mValue) {
        x /= mValue;  y /= mValue;
    }

    T operator * (const vec2_t& v) const {
        return x*v.x + y*v.y;
    }

    static const vec2_t zero;
    static const vec2_t unit;
    static const vec2_t unit_x;
    static const vec2_t unit_y;

    T x, y;
};

template<typename T>
const vec2_t<T> vec2_t<T>::zero(0, 0);

template<typename T>
const vec2_t<T> vec2_t<T>::unit(1, 1);

template<typename T>
const vec2_t<T> vec2_t<T>::unit_x(1, 0);

template<typename T>
const vec2_t<T> vec2_t<T>::unit_y(0, 1);

using vec2f  = vec2_t<float>;
using vec2d  = vec2_t<double>;
using vec2i  = vec2_t<std::ptrdiff_t>;
using vec2ui = vec2_t<std::size_t>;

template<typename T, typename N>
vec2_t<T> operator * (N a, const vec2_t<T>& v) {
    return vec2_t<T>(v.x*a, v.y*a);
}

template<typename T>
std::ostream& operator << (std::ostream& s, const vec2_t<T>& v) {
    return s << v.x << ", " << v.y;
}

template<typename T>
std::istream& operator >> (std::istream& s, vec2_t<T>& v) {
    char delim;
    return s >> v.x >> delim >> v.y;
}

template<typename T>
sf::Packet& operator << (sf::Packet& s, const vec2_t<T>& v) {
    return s << v.x << v.y;
}

template<typename T>
sf::Packet& operator >> (sf::Packet& s, vec2_t<T>& v) {
    return s >> v.x >> v.y;
}

#endif
