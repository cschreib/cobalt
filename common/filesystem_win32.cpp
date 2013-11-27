#include "filesystem.hpp"
#include "string.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <windows.h>
#include <direct.h>
#include <io.h>

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
}
