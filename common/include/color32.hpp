#ifndef GRAPHICS_COLOR_HPP
#define GRAPHICS_COLOR_HPP

#include <iosfwd>
#include <cstdint>

struct hls_color {
    using chanel = std::uint8_t;
    chanel h, l, s, a;
};

struct color32 {
public :
    using chanel = std::uint8_t;

    color32();
    color32(chanel nr, chanel ng, chanel nb, chanel na = 255u);

    chanel*       data();
    const chanel* data() const;

    hls_color to_hls() const;
    void      from_hls(const hls_color& hls);

    color32 luminosity(float f) const;
    color32 saturation(float f) const;
    color32 hue(float f) const;

    color32 alpha_blend(float f) const;

    bool operator == (const color32& c2) const;
    bool operator != (const color32& c2) const;

    void operator += (const color32& c2);
    void operator -= (const color32& c2);
    void operator *= (const color32& c2);
    void operator *= (float f);

    chanel r, g, b, a;

    static const color32 empty;
    static const color32 white;
    static const color32 black;
    static const color32 red;
    static const color32 green;
    static const color32 blue;
    static const color32 grey;
};

color32 operator + (const color32& c1, const color32& c2);
color32 operator - (const color32& c1, const color32& c2);
color32 operator * (const color32& c1, const color32& c2);
color32 operator * (const color32& c1, float f);
color32 operator * (float f, const color32& c2);

std::ostream& operator << (std::ostream& s, const color32& c);
std::istream& operator >> (std::istream& s, color32& c);

#endif
