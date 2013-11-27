#ifndef STRINGIFY_HPP
#define STRINGIFY_HPP

#include <string>
#include <vector>
#include <sstream>

template<typename T>
struct stringify {
    static bool parse(T& i, const std::string& value) {
        std::istringstream ss(value);
        ss >> i;
        return !ss.fail();
    }
    static void serialize(const T& i, std::string& value) {
        std::ostringstream ss;
        ss << i;
        value = ss.str();
    }
};

template<>
struct stringify<char> {
    static bool parse(char& i, const std::string& value) {
        std::istringstream ss(value);
        int ti;
        ss >> ti;
        if (!ss.fail()) {
            if (ti < -128) i = -128; else if (ti > 127) i = 127; else i = ti;
            return true;
        } else {
            return false;
        }
    }
    static void serialize(char i, std::string& value) {
        std::ostringstream ss;
        ss << (int)i;
        value = ss.str();
    }
};

template<>
struct stringify<unsigned char> {
    static bool parse(unsigned char& i, const std::string& value) {
        std::istringstream ss(value);
        unsigned int ti;
        ss >> ti;
        if (!ss.fail()) {
            if (ti > 255) i = 255; else i = ti;
            return true;
        } else {
            return false;
        }
    }
    static void serialize(unsigned char i, std::string& value) {
        std::ostringstream ss;
        ss << (unsigned int)i;
        value = ss.str();
    }
};

template<>
struct stringify<std::string> {
    static bool parse(std::string& s, const std::string& value) {
        s = value;
        return true;
    }
    static void serialize(const std::string& s, std::string& value) {
        value = s;
    }
};

template<>
struct stringify<bool> {
    static bool parse(bool& s, const std::string& value) {
        if (value == "true" || value == "1") {
            s = true;
            return true;
        } else if (value == "false" || value == "0") {
            s = false;
            return true;
        }
        return false;
    }
    static void serialize(bool s, std::string& value) {
        value = s ? "true" : "false";
    }
};

template<typename T>
struct stringify<std::vector<T>> {
    static bool parse(std::vector<T>& v, const std::string& value) {
        std::size_t last = 0;
        do {
            if (last != 0) ++last;
            std::size_t end = value.find_first_of(',', last);
            T tmp;
            std::size_t len = end;
            if (end != value.npos) len -= last;
            if (!stringify<T>::parse(tmp, value.substr(last, len))) return false;
            v.push_back(tmp);
            last = end;
        } while (last != value.npos);

        return true;
    }
    static void serialize(const std::vector<T>& v, std::string& value) {
        for (std::size_t i = 0; i < v.size(); ++i) {
            if (i != 0) value += ", ";
            std::string tmp;
            stringify<T>::serialize(v[i], tmp);
            value += tmp;
        }
    }
};

template<typename T>
std::string to_string(const T& t) {
    std::string str;
    stringify<T>::serialize(t, str);
    return str;
}

#endif
