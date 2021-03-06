#include "filesystem.hpp"
#include "string.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <fstream>

namespace file {
    std::vector<std::string> list_directories(const std::string& path = "") {
        std::vector<std::string> dlist;

        long handle, res;
        struct _finddata_t tagData;

        std::string pattern;
        if (path.empty()) {
            pattern = "*";
        } else {
            pattern = path + "/*";
        }

        handle = _findfirst(pattern.c_str(), &tagData);
        res = 0;
        while (handle != -1 && res != -1) {
            if ((tagData.attrib & _A_HIDDEN) != 0) {
                res = _findnext(handle, &tagData);
                continue;
            }

            if ((tagData.attrib & _A_SUBDIR) != 0) {
                std::string s = tagData.name;
                if (s != "." && s != "..") {
                    dlist.data.push_back(s);
                }
            }

            res = _findnext(handle, &tagData);
        }

        if (handle != -1) {
            _findclose(handle);
        }

        dlist.dims[0] = dlist.size();
        return dlist;
    }

    std::vector<std::string> list_files(const std::string& pattern = "*") {
        std::vector<std::string> flist;

        long handle, res;
        struct _finddata_t tagData;

        handle = _findfirst(pattern.c_str(), &tagData);
        res = 0;
        while (handle != -1 && res != -1) {
            if ((tagData.attrib & _A_HIDDEN) != 0) {
                res = _findnext(handle, &tagData);
                continue;
            }

            if ((tagData.attrib & _A_SUBDIR) == 0) {
                flist.data.push_back(tagData.name);
            }

            res = _findnext(handle, &tagData);
        }

        if (handle != -1) {
            _findclose(handle);
        }

        flist.dims[0] = flist.size();
        return flist;
    }

    bool mkdir_(const std::string& path) {
        return ;
    }

    bool mkdir(const std::string& tpath) {
        std::string path = string::replace(string::trim(tpath, " \t"), "\\", "/");
        std::vector<std::string> dirs = string::split(path, "/");
        path = "";

        // Take care of the drive letter
        if (!dirs.empty() && !dirs.front().empty() && dirs.front().back() == ':') {
            path = dirs.front()+'/';
            dirs.erase(dirs.begin());
        }

        for (auto& d : dirs) {
            if (d.empty()) continue;

            if (path.empty()) {
                path = d;
            } else {
                path += "/" + d;
            }

            bool res = (CreateDirectory(path.c_str(), 0) != 0) ||
                (GetLastError() == ERROR_ALREADY_EXISTS);

            if (!res) return false;
        }

        return true;
    }

    bool is_older(const std::string& file1, const std::string& file2) {
        WIN32_FILE_ATTRIBUTE_DATA st1 = {0};
        WIN32_FILE_ATTRIBUTE_DATA st2 = {0};
        GetFileAttributesEx(file1.c_str(), GetFileExInfoStandard, &st1);
        GetFileAttributesEx(file2.c_str(), GetFileExInfoStandard, &st2);
        return CompareFileTime(&st1.ftCreationTime, &st2.ftCreationTime) < 0;
    }
}

shared_library::shared_library(const std::string& file) {
    handle_ = LoadLibrary(file.c_str());
}

shared_library::~shared_library() {
    FreeLibrary(handle_);
}

shared_library::shared_library(shared_library&& s) : handle_(s.handle_) {
    s.handle_ = nullptr;
}

shared_library& shared_library::operator=(shared_library&& s) {
    handle_ = s.handle_;
    s.handle_ = nullptr;
    return *this;
}

bool shared_library::open() const {
    return handle_ != nullptr;
}

void* shared_library::load_symbol(const std::string& sym) {
    if (handle_) {
        return GetProcAddress(handle_, sym.c_str());
    } else {
        return nullptr;
    }
}

const std::string shared_library::file_extension = "dll";
