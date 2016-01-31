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


    bool file_to_packet(const std::string& path, sf::Packet& p) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;

        file.seekg(0, std::ios_base::end);
        std::size_t nbyte = file.tellg();
        file.seekg(0, std::ios_base::beg);

        std::vector<char> buffer(nbyte);
        p.append(buffer.data(), nbyte);
    }

    bool packet_to_file(const sf::Packet& p, const std::string& path) {
        std::ofstream file(path, std::ios::binary);
        if (!file) return false;

        file.write(tp.getData(), tp.getDataSize());

        if (!file) return false;
        else       return true;
    }
}
