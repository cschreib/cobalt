#ifndef AXIS_ALIGNED_BOX2_HPP
#define AXIS_ALIGNED_BOX2_HPP

#include "vector2.hpp"
#include <limits>
#include <algorithm>
#include <iostream>

namespace axis_aligned_box2_impl {
    template<typename T, bool has_inf, bool is_signed>
    struct type_traits;

    template<typename T>
    struct type_traits<T, true, true> {
        static constexpr T min = -std::numeric_limits<T>::infinity();
        static constexpr T max = +std::numeric_limits<T>::infinity();
    };
    template<typename T>
    struct type_traits<T, true, false> {
        static constexpr T min = 0;
        static constexpr T max = std::numeric_limits<T>::infinity();
    };
    template<typename T>
    struct type_traits<T, false, true> {
        static constexpr T min = std::numeric_limits<T>::min();
        static constexpr T max = std::numeric_limits<T>::max();
    };
    template<typename T>
    struct type_traits<T, false, false> {
        static constexpr T min = 0;
        static constexpr T max = std::numeric_limits<T>::max();
    };
}

template<typename T>
struct axis_aligned_box2_t {
    using point = vec2_t<T>;
    using type_traits = axis_aligned_box2_impl::type_traits<T,
        std::numeric_limits<T>::has_infinity,
        std::numeric_limits<T>::is_signed
    >;

    axis_aligned_box2_t() :
        p1(type_traits::min, type_traits::min),
        p2(type_traits::max, type_traits::max) {}

    axis_aligned_box2_t(const point& p1, const point& p2) : p1(p1), p2(p2) {}

    axis_aligned_box2_t(const point& center, T width, T height) :
        p1(center - point(width/2.0, height/2.0)),
        p2(center + point(width/2.0, height/2.0)) {}

    bool contains(const point& p) const {
        return (p1.x <= p.x && p.x <= p2.x && p1.y <= p.y && p.y <= p2.y);
    }

    bool contains(const axis_aligned_box2_t& m) const {
        point dist = m.center() - center();
        T mindx = m.width()  + width();
        T mindy = m.height() + height();

        return fabs(dist.x) < mindx/2.0 && fabs(dist.y) < mindy/2.0;
    }

    void grow(const point& p) {
        p2.x = std::max(p.x, p2.x);
        p2.y = std::max(p.y, p2.y);
        p1.x = std::min(p.x, p1.x);
        p1.y = std::min(p.y, p1.y);
    }

    point center() const {
        return (p1 + p2)/2.0;
    }

    T width() const {
        return p2.x - p1.x;
    }

    T height() const {
        return p2.y - p1.y;
    }

    point operator [] (std::size_t i) const {
        switch (i) {
        case 0 :  return p1;                break;
        case 1 :  return point(p2.x, p1.y); break;
        case 2 :  return p2;                break;
        case 3 :  return point(p1.x, p2.y); break;
        default : return point::zero;
        }
    }

    axis_aligned_box2_t operator + (const point& v) const {
        return axis_aligned_box2_t(p1 + v, p2 + v);
    }
    friend axis_aligned_box2_t operator + (const point& v, const axis_aligned_box2_t& b) {
        return b + v;
    }
    void operator += (const point& v) {
        p1 += v; p2 += v;
    }

    axis_aligned_box2_t operator - (const point& v) const {
        return axis_aligned_box2_t(p1 - v, p2 - v);
    }
    void operator -= (const point& v) {
        p1 -= v; p2 -= v;
    }

    axis_aligned_box2_t operator * (T f) const {
        axis_aligned_box2_t b(*this);
        b *= f;
        return b;
    }
    friend axis_aligned_box2_t operator * (T f, const axis_aligned_box2_t& b) {
        return b*f;
    }
    void operator *= (T f) {
        point c = center();
        T w = width()*f, h = height()*f;
        p1 = c - point(w/2.0, h/2.0);
        p2 = c + point(w/2.0, h/2.0);
    }

    axis_aligned_box2_t operator / (T f) const {
        axis_aligned_box2_t b(*this);
        b /= f;
        return b;
    }
    void operator /= (T f) {
        point c = center();
        T w = width()/f, h = height()/f;
        p1 = c - point(w/2.0, h/2.0);
        p2 = c + point(w/2.0, h/2.0);
    }

    point p1, p2;
};

using axis_aligned_box2f = axis_aligned_box2_t<float>;
using axis_aligned_box2d = axis_aligned_box2_t<double>;
using axis_aligned_box2i = axis_aligned_box2_t<int>;

template<class O, class T>
O& operator << (O& s, const axis_aligned_box2_t<T>& b) {
    return s << b.p1 << ", " << b.p2;
}

#endif
