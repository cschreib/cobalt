#include "color32.hpp"
#include <iostream>
#include <sstream>

const color32 color32::empty(0, 0, 0, 0);
const color32 color32::white(255, 255, 255);
const color32 color32::black(0, 0, 0);
const color32 color32::red  (255, 0, 0);
const color32 color32::green(0, 255, 0);
const color32 color32::blue (0, 0, 255);
const color32 color32::grey (128, 128, 128);

color32::color32() {}

color32::color32(chanel nr, chanel ng, chanel nb, chanel na) :
    r(nr), g(ng), b(nb), a(na) {}

color32::chanel* color32::data() {
    return &r;
}

const color32::chanel* color32::data() const {
    return &r;
}

hls_color color32::to_hls() const {
    chanel ma = std::max(std::max(r, g), b);
    chanel mi = std::min(std::min(r, g), b);

    hls_color hls;
    hls.a = a;

    if (ma == mi) {
        hls.l = ma/255.0f;
        hls.s = 0.0f;
    } else {
        float delta = ma - mi;
        float sum   = ma + mi;

        hls.l = 0.5f*sum/255.0f;
        if (hls.l < 0.5f) {
            hls.s = delta/sum;
        } else {
            hls.s = delta/(2.0f*255.0f - sum);
        }

        if      (ma == r) hls.h = 60.0f*(g - b)/delta +   0.0f;
        else if (ma == g) hls.h = 60.0f*(b - r)/delta + 120.0f;
        else if (ma == b) hls.h = 60.0f*(r - g)/delta + 240.0f;


        if      (hls.h < 0.0f)   hls.h = hls.h + 360.0f;
        else if (hls.h > 360.0f) hls.h = hls.h - 360.0f;
    }

    return hls;
}

float h2_to_rgb(float v1, float v2, float h) {
    if      (h < 0.0f)   h = h + 360.0f;
    else if (h > 360.0f) h = h - 360.0f;

    if      (h < 60.0f)  return v1 + (v2 - v1)*h/60.0f;
    else if (h < 180.0f) return v2;
    else if (h < 240.0f) return v1 + (v2 - v1)*(4.0f - h/60.0f);
    else                 return v1;
}

void color32::from_hls(const hls_color& hls) {
    a = hls.a;

    if (hls.s == 0.0f) {
        r = 255.0f*hls.l; g = 255.0f*hls.l; b = 255.0f*hls.l;
    } else {
        float v2;
        if (hls.l < 0.5f) {
            v2 = hls.l*(1.0f + hls.s);
        } else {
            v2 = hls.l + hls.s - hls.l*hls.s;
        }

        float v1 = 2.0f*hls.l - v2;

        r = 255.0f*h2_to_rgb(v1, v2, hls.h + 120.0f);
        g = 255.0f*h2_to_rgb(v1, v2, hls.h);
        b = 255.0f*h2_to_rgb(v1, v2, hls.h - 120.0f);
    }
}

color32 color32::luminosity(float f) const {
    hls_color hls = to_hls();
    hls.l = f;
    color32 c; c.from_hls(hls);
    return c;
}

color32 color32::saturation(float f) const {
    hls_color hls = to_hls();
    hls.s = f;
    color32 c; c.from_hls(hls);
    return c;
}
color32 color32::hue(float f) const {
    hls_color hls = to_hls();
    hls.h = f;
    color32 c; c.from_hls(hls);
    return c;
}

color32 color32::alpha_blend(float f) const {
    return color32(r, g, b, a*f);
}

bool color32::operator == (const color32& c2) const {
    return (r == c2.r && g == c2.g && b == c2.b && a == c2.a);
}

bool color32::operator != (const color32& c2) const {
    return (r != c2.r || g != c2.g || b != c2.b || a != c2.a);
}

color32::chanel add(color32::chanel a, color32::chanel b) {
    if (255u - a < b) return 255u;
    return a + b;
}

color32::chanel sub(color32::chanel a, color32::chanel b) {
    if (a < b) return 0u;
    return a - b;
}

color32::chanel mul(color32::chanel a, color32::chanel b) {
    return a*(b/255.0f);
}

void color32::operator += (const color32& c2) {
    r = add(r,c2.r); g = add(g,c2.g); b = add(b,c2.b);
}

void color32::operator -= (const color32& c2) {
    r = sub(r,c2.r); g = sub(g,c2.g); b = sub(b,c2.b);
}

void color32::operator *= (const color32& c2) {
    r = mul(r,c2.r); g = mul(g,c2.g); b = mul(b,c2.b);
}

void color32::operator *= (float f) {
    r *= f; g *= f; b *= f;
}

color32 operator + (const color32& c1, const color32& c2) {
    color32 c = c1; c += c2; return c;
}

color32 operator - (const color32& c1, const color32& c2) {
    color32 c = c1; c -= c2; return c;
}

color32 operator * (const color32& c1, const color32& c2) {
    color32 c = c1; c *= c2; return c;
}

color32 operator * (const color32& c1, float f) {
    color32 c = c1; c *= f; return c;
}

color32 operator * (float f, const color32& c2) {
    color32 c = c2; c *= f; return c;
}

std::string uchar_to_hex(std::uint8_t i) {
    std::ostringstream ss;
    ss << std::hex << (std::size_t)i;
    std::string res = ss.str();
    if (res.size() != 2) {
        res = '0' + res;
    }

    return res;
}

std::string to_string(const color32& c) {
    std::string str = "#" + uchar_to_hex(c.r) + uchar_to_hex(c.g) + uchar_to_hex(c.b);
    if (c.a != 255u) {
        str += uchar_to_hex(c.a);
    }
    return str;
}

std::ostream& operator << (std::ostream& s, const color32& c) {
    s << to_string(c);
    return s;
}

std::size_t hex_to_uint(const std::string& s) {
    std::size_t i;
    std::istringstream ss(s);
    ss >> std::hex >> i;
    return i;
}

std::istream& operator >> (std::istream& s, color32& c) {
    auto pos = s.tellg();
    char ch; s >> ch;
    if (ch == '#') {
        char h[3]; h[2] = '\0';
        s >> h[0] >> h[1];
        c.r = hex_to_uint(h);
        s >> h[0] >> h[1];
        c.g = hex_to_uint(h);
        s >> h[0] >> h[1];
        c.b = hex_to_uint(h);

        pos = s.tellg();
        if (!s.eof()) {
            s >> h[0];
            if (isalnum(h[0]) && !s.eof()) {
                s >> h[1];
                if (isalnum(h[1])) {
                    c.a = hex_to_uint(h);
                    return s;
                }
            }
        }

        s.seekg(pos);
        c.a = 255u;
    } else {
        s.seekg(pos);
        char delim;
        s >> c.r >> delim >> c.g >> delim >> c.b >> delim >> c.a;
    }

    return s;
}
