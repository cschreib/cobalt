#include "filesystem.hpp"
#include "string.hpp"
#include <fstream>
#include <SFML/Network/Packet.hpp>

namespace file {
    bool exists(const std::string& file) {
        if (file.empty()) {
            return false;
        }

        std::ifstream f(file.c_str());
        return f.is_open();
    }

    std::string directorize(const std::string& path) {
        std::string dir = string::trim(path);
        if (!dir.empty() && dir.back() != '/') {
            dir.push_back('/');
        }

        return dir;
    }

    std::string get_directory(const std::string& path) {
        auto pos = path.find_last_of('/');
        if (pos == path.npos) {
            return "./";
        } else if (pos == path.find_last_not_of(' ')) {
            if (pos == 0) {
                return "/";
            } else {
                pos = path.find_last_of('/', pos-1);
                if (pos == path.npos) {
                    return "./";
                } else {
                    return path.substr(0, pos+1);
                }
            }
        } else {
            return path.substr(0, pos+1);
        }
    }

    std::string get_basename(std::string path) {
        auto pos = path.find_last_of('/');
        if (pos == path.npos) {
            return path;
        } else {
            auto lpos = path.find_last_not_of(' ');
            if (pos == lpos) {
                if (pos == 0) {
                    return "/";
                } else {
                    pos = path.find_last_of('/', pos-1);
                    if (pos == path.npos) {
                        return path.substr(lpos);
                    } else {
                        return path.substr(pos+1, lpos-pos-1);
                    }
                }
            } else {
                return path.substr(pos+1);
            }
        }
    }

    std::string remove_extension(std::string s) {
        auto p = s.find_last_of('.');
        if (p == s.npos) return s;
        return s.substr(0u, p);
    }

    std::string get_extension(std::string s) {
        auto p = s.find_last_of('.');
        if (p == s.npos) return "";
        return s.substr(p);
    }

    std::pair<std::string,std::string> split_extension(std::string s) {
        auto p = s.find_last_of('.');
        if (p == s.npos) return std::make_pair(s, std::string{});
        return std::make_pair(s.substr(0u, p), s.substr(p));
    }
}
