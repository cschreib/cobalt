#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include <string>
#include <vector>

namespace file {
    bool exists(const std::string& file);
    std::vector<std::string> list_directories(const std::string& path = "");
    std::vector<std::string> list_files(const std::string& pattern = "*");
    std::string directorize(const std::string& path);
    std::string get_directory(const std::string& file);
    bool mkdir(const std::string& path);
}

#endif
