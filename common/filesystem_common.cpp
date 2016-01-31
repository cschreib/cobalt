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

    std::string get_directory(const std::string& file) {
        std::vector<std::string> tree = string::split(file, "/");
        if (tree.size() == 1) return "./";
        return file.substr(0, file.size()-tree[tree.size()-1].size());
    }
}
