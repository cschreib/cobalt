#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include <string>
#include <vector>

namespace sf {
    class Packet;
}

class shared_library {
private :
    void* handle_ = nullptr;
public :
    shared_library() = default;
    explicit shared_library(const std::string& file);
    ~shared_library();

    shared_library(const shared_library&) = delete;
    shared_library& operator=(const shared_library&) = delete;

    shared_library(shared_library&&);
    shared_library& operator=(shared_library&&);

    bool open() const;
    void* load_symbol(const std::string& sym);

    template<typename T>
    T* load_function(const std::string& name) {
        return reinterpret_cast<T*>(load_symbol(name));
    }

    static const std::string file_extension;
};

namespace file {
    bool exists(const std::string& file);
    std::vector<std::string> list_directories(const std::string& path = "");
    std::vector<std::string> list_files(const std::string& pattern = "*");
    bool mkdir(const std::string& path);
    bool remove(const std::string& path);
    bool is_older(const std::string& file1, const std::string& file2);

    std::string directorize(const std::string& path);
    std::string get_directory(const std::string& file);
    std::string get_basename(std::string path);
    std::string remove_extension(std::string s);
    std::string get_extension(std::string s);
    std::pair<std::string,std::string> split_extension(std::string s);
}

#endif
