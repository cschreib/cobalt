#include "string.hpp"
#include "range.hpp"

namespace string {
    std::string trim(std::string s, const std::string& chars) {
        std::size_t spos = s.find_first_of(chars);
        if (spos == 0) {
            std::size_t epos = s.find_first_not_of(chars);
            if (epos == s.npos) return "";
            s = s.substr(epos);
        }

        spos = s.find_last_of(chars);
        if (spos == s.size()-1) {
            std::size_t epos = s.find_last_not_of(chars);
            s = s.erase(epos+1, s.size() - epos+1);
        }

        return s;
    }

    std::string join(const std::vector<std::string>& vs, const std::string& delim) {
        if (vs.empty()) return "";
        std::string res = vs[0];
        for (std::size_t i : range(1, vs)) {
            res += delim + vs[i];
        }

        return res;
    }

    std::string to_upper(std::string s) {
        for (auto& c : s) {
            c = ::toupper(c);
        }

        return s;
    }

    std::string to_lower(std::string s) {
        for (auto& c : s) {
            c = ::tolower(c);
        }

        return s;
    }

    std::string erase_begin(std::string s, std::size_t n) {
        if (n >= s.size()) {
            return "";
        }

        return s.erase(0, n);
    }

    std::string erase_end(std::string s, std::size_t n) {
        if (n >= s.size()) {
            return "";
        }

        return s.erase(s.size()-n, n);
    }

    std::string replace(std::string s, const std::string& pattern, const std::string& rep) {
        auto p = s.find(pattern);
        while (p != s.npos) {
            s.replace(p, pattern.size(), rep);
            p = s.find(pattern, p+rep.size());
        }

        return s;
    }

    std::size_t distance(const std::string& t, const std::string& u) {
        std::size_t n = std::min(t.size(), u.size());
        std::size_t d = std::max(t.size(), u.size()) - n;
        for (std::size_t i : range(n)) {
            if (t[i] != u[i]) ++d;
        }

        return d;
    }

    bool start_with(const std::string& s, const std::string& pattern) {
        if (s.size() < pattern.size()) return false;
        for (std::size_t i : range(pattern)) {
            if (s[i] != pattern[i]) return false;
        }

        return true;
    }

    bool end_with(const std::string& s, const std::string& pattern) {
        if (s.size() < pattern.size()) return false;
        for (std::size_t i : range(1, pattern)) {
            if (s[s.size()-i] != pattern[pattern.size()-i]) return false;
        }

        return true;
    }

    std::vector<std::string> split(const std::string& s, const std::string& pattern) {
        std::vector<std::string> ret;
        std::size_t p = 0, op = 0;
        while ((p = s.find(pattern, op)) != s.npos) {
            ret.push_back(s.substr(op, p - op));
            op = p+pattern.size();
        }

        ret.push_back(s.substr(op));

        return ret;
    }

    std::string collapse(const std::vector<std::string>& sv, const std::string& sep) {
        std::string ret = sv[0];
        for (std::size_t i : range(1, sv)) {
            ret += sep;
            ret += sv[1];
        }

        return ret;
    }

    std::string uchar_to_hex(std::uint8_t i) {
        std::ostringstream ss;
        ss << std::hex << static_cast<std::size_t>(i);
        std::string res = ss.str();
        if (res.size() != 2) {
            res = '0' + res;
        }

        return res;
    }

    std::uint8_t hex_to_uchar(std::string s) {
        std::size_t v = 0;
        std::istringstream ss(s);
        ss >> std::hex >> v;
        return v;
    }

    void to_utf8_(std::string& res, unicode_char c) {
        static unicode_char MAX_ANSI = 127;
        static unicode_char ESC_C2   = 194;
        static unicode_char ESC_C3   = 195;

        if (c <= MAX_ANSI) {
            res.push_back(c);
        } else {
            if (c < 192) {
                res.push_back((unsigned char)ESC_C2);
                res.push_back((unsigned char)(128 + c - 128));
            } else {
                res.push_back((unsigned char)ESC_C3);
                res.push_back((unsigned char)(128 + c - 192));
            }
        }
    }

    std::string to_utf8(unicode_char c) {
        std::string res;
        to_utf8_(res, c);
        return res;
    }

    std::string to_utf8(const unicode& s) {
        std::string res;
        for (auto c : s) {
            to_utf8_(res, c);
        }

        return res;
    }

    unicode_char to_unicode(unsigned char c) {
        static unsigned char MAX_ANSI = 127;

        unicode_char res;

        if (c <= MAX_ANSI) {
            res = c;
        } else {
            res = 0;
        }

        return res;
    }

    unicode to_unicode(const std::string& s) {
        static unsigned char MAX_ANSI = 127;
        static unsigned char ESC_C2   = 194;
        static unsigned char ESC_C3   = 195;

        unicode res;

        unsigned char esc = 0;

        for (auto c : s) {
            if (c <= MAX_ANSI) {
                res.push_back(c);
            } else {
                if (c == ESC_C2 || c == ESC_C3) {
                    esc = c;
                    continue;
                }

                if (esc != 0) {
                    if (esc == ESC_C3) {
                        res.push_back(192 + c - 128);
                    } else if (esc == ESC_C2) {
                        res.push_back(128 + c - 128);
                    }

                    esc = 0;
                }
            }
        }

        return res;
    }
}
