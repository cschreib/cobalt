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
}
