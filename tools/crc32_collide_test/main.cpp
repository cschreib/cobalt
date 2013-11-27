#include <crc32.hpp>
#include <string.hpp>
#include <print.hpp>
#include <filesystem.hpp>
#include <fstream>
#include <iostream>
#include <vector>

struct packet {
    std::string name;
    std::string loc;
};

void seek_packets(std::ifstream& f, const std::string& file, std::vector<packet>& packets) {
    if (!f.is_open()) return;

    std::string line;
    std::size_t lcnt = 0;
    while (!f.eof()) {
        ++lcnt;
        std::getline(f, line);
        
        std::size_t pos = line.find("ID_STRUCT");
        if (pos == line.npos) continue;

        std::size_t start = line.find_first_of('(', pos);
        if (start == line.npos) continue;
        ++start;

        std::size_t end = line.find_first_of(')', start);
        if (end == line.npos) continue;

        packets.push_back({line.substr(start, end-start), file+":"+string::convert(lcnt)});
    }
}

void seek_packets(const std::string& dir, std::vector<packet>& packets) {
    std::vector<std::string> files = file::list_files(dir+"/*.hpp");
    for (auto& f : files) {
        std::ifstream fs(dir+"/"+f);
        seek_packets(fs, dir+"/"+f, packets);
    }

    std::vector<std::string> dirs = file::list_directories(dir);
    for (auto& d : dirs) {
        seek_packets(dir+"/"+d, packets);
    }
}

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        std::cout << "crc32_collide_test: missing work folder" << std::endl;
        return 1;
    }

    std::string dir = argv[1];
    dir = string::replace(dir, "\\", "/");
    auto p = dir.find_last_not_of('/');
    if (p != dir.npos) {
        dir.erase(p+1);
    }

    std::vector<std::string> dirs = {
        dir+"/common/include", 
        dir+"/server/include",
        dir+"/client/include"
    };

    std::vector<packet> packets;
    for (auto& d : dirs) {
        seek_packets(d, packets);
    }

    std::cout << "crc32_collide_test: found " << packets.size() << " packets" << std::endl;

    std::vector<std::uint32_t> crcs;
    crcs.reserve(packets.size());
    for (auto& p : packets) {
        crcs.push_back(get_crc32(p.name));
    }
    
    bool error = false;
    for (std::size_t i = 0;   i < packets.size(); ++i)
    for (std::size_t j = i+1; j < packets.size(); ++j) {
        if (crcs[i] == crcs[j]) {
            error = true;
            std::cout << "crc32_collide_test: collision detected:" << std::endl;
            std::cout << "  " << packets[i].loc << ": " << packets[i].name << std::endl;
            std::cout << "  " << packets[j].loc << ": " << packets[j].name << std::endl;
        }
    }

    if (!error) {
        std::cout << "crc32_collide_test: no collision found" << std::endl;
    }

    return error;
}
