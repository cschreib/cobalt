#ifndef STRING_HPP
#define STRING_HPP

#include <string>
#include <vector>
#include <sstream>
#include <cmath>

namespace string {
    template<typename T>
    std::string convert(const T& t) {
        std::ostringstream ss;
        ss << t;
        return ss.str();
    }

    template<typename T, typename enable = typename std::enable_if<!std::is_same<T,std::string>::value>::type>
    std::string convert(const T& t, std::size_t n, char fill = '0') {
        if (n <= 1) {
            return convert(t);
        }
        if (t == 0) {
            return std::string(n-1, fill)+'0';
        } else {
            std::ostringstream ss;
            ss << t;
            std::size_t nz = n-1 - floor(log10(t));
            if (nz > 0 && nz < 6) {
                return std::string(nz, fill) + ss.str();
            } else {
                return ss.str();
            }
        }
    }
    
    template<typename T>
    bool convert(const std::string& s, T& t) {
        std::istringstream ss(s);
        return (ss >> t);
    }

    std::string trim(std::string ts, const std::string& chars = " \t");
    std::string join(const std::vector<std::string>& vs, const std::string& delim = "");
    std::string to_upper(std::string ts);
    std::string to_lower(std::string ts);
    std::string erase_begin(std::string ts, std::size_t n = 1);
    std::string erase_end(std::string ts, std::size_t n = 1);
    std::string replace(std::string ts, const std::string& pattern, const std::string& rep);
    std::size_t distance(const std::string& t, const std::string& u);
    bool start_with(const std::string& s, const std::string& pattern);
    bool end_with(const std::string& s, const std::string& pattern);
    std::vector<std::string> split(const std::string& ts, const std::string& pattern);
}

#endif
