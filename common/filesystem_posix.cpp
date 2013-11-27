#include "filesystem.hpp"
#include "string.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fnmatch.h>
#include <cstring>
#include <cstdio>

namespace file {
    namespace impl {
        struct _finddata_t {
            char *name;
            int attrib;
            unsigned long size;
        };

        struct _find_search_t {
            char *pattern;
            char *curfn;
            char *directory;
            int dirlen;
            DIR *dirfd;
        };

        #define _A_NORMAL 0x00 /* Normalfile-Noread/writerestrictions */
        #define _A_RDONLY 0x01 /* Read only file */
        #define _A_HIDDEN 0x02 /* Hidden file */
        #define _A_SYSTEM 0x04 /* System file */
        #define _A_SUBDIR 0x10 /* Subdirectory */
        #define _A_ARCH 0x20 /* Archive file */

        int _findclose(long id);
        int _findnext(long id, struct _finddata_t *data);

        long _findfirst(const char *pattern, struct _finddata_t *data) {
            _find_search_t *fs = new _find_search_t;
            fs->curfn = NULL;
            fs->pattern = NULL;

            const char *mask = strrchr(pattern, '/');
            if (mask) {
                fs->dirlen = mask - pattern;
                mask++;
                fs->directory = (char *)malloc(fs->dirlen + 1);
                memcpy(fs->directory, pattern, fs->dirlen);
                fs->directory[fs->dirlen] = 0;
            } else {
                mask = pattern;
                fs->directory = strdup(".");
                fs->dirlen = 1;
            }

            fs->dirfd = opendir(fs->directory);
            if (!fs->dirfd) {
                _findclose((long)fs);
                return -1;
            }

            if (strcmp(mask, "*.*") == 0) {
                mask += 2;
            }

            fs->pattern = strdup(mask);

            if (_findnext((long)fs, data) < 0) {
                _findclose((long)fs);
                return -1;
            }

            return (long)fs;
        }

        int _findnext(long id, struct _finddata_t *data) {
            _find_search_t *fs = (_find_search_t*)id;

            dirent *entry;
            for (;;) {
                if (!(entry = readdir(fs->dirfd))) {
                    return -1;
                }

                if (fnmatch(fs->pattern, entry->d_name, 0) == 0) {
                    break;
                }
            }

            if (fs->curfn) {
                free(fs->curfn);
            }

            data->name = fs->curfn = strdup(entry->d_name);

            size_t namelen = strlen(entry->d_name);
            char *xfn = new char[fs->dirlen + 1 + namelen + 1];
            sprintf(xfn, "%s/%s", fs->directory, entry->d_name);

            struct stat stat_buf;
            if (stat(xfn, &stat_buf)) {
                data->attrib = _A_NORMAL;
                data->size = 0;
            } else {
                if (S_ISDIR(stat_buf.st_mode)) {
                    data->attrib = _A_SUBDIR;
                } else {
                    data->attrib = _A_NORMAL;
                }

                data->size = stat_buf.st_size;
            }

            delete[] xfn;

            if (data->name [0] == '.') {
                data->attrib |= _A_HIDDEN;
            }

            return 0;
        }

        int _findclose(long id) {
            int ret;
            _find_search_t *fs = (_find_search_t *)id;

            ret = fs->dirfd ? closedir(fs->dirfd) : 0;
            free(fs->pattern);
            free(fs->directory);
            if (fs->curfn)
                free(fs->curfn);
            delete fs;

            return ret;
        }
    }

    std::vector<std::string> list_directories(const std::string& path) {
        std::vector<std::string> dlist;

        long handle, res;
        struct impl::_finddata_t tagData;

        std::string pattern;
        if (path.empty()) {
            pattern = "*";
        } else {
            pattern = path + "/*";
        }

        handle = impl::_findfirst(pattern.c_str(), &tagData);
        res = 0;
        while (handle != -1 && res != -1) {
            if ((tagData.attrib & _A_HIDDEN) != 0) {
                res = impl::_findnext(handle, &tagData);
                continue;
            }

            if ((tagData.attrib & _A_SUBDIR) != 0) {
                std::string s = tagData.name;
                if (s != "." && s != "..") {
                    dlist.push_back(s);
                }
            }

            res = impl::_findnext(handle, &tagData);
        }

        if (handle != -1) {
            impl::_findclose(handle);
        }

        return dlist;
    }

    std::vector<std::string> list_files(const std::string& pattern) {
        std::vector<std::string> flist;

        long handle, res;
        struct impl::_finddata_t tagData;

        handle = impl::_findfirst(pattern.c_str(), &tagData);
        res = 0;
        while (handle != -1 && res != -1) {
            if ((tagData.attrib & _A_HIDDEN) != 0) {
                res = impl::_findnext(handle, &tagData);
                continue;
            }

            if ((tagData.attrib & _A_SUBDIR) == 0) {
                flist.push_back(tagData.name);
            }

            res = impl::_findnext(handle, &tagData);
        }

        if (handle != -1) {
            impl::_findclose(handle);
        }

        return flist;
    }

    bool mkdir(const std::string& tpath) {
        std::string path = string::replace(string::trim(tpath, " \t"), "\\", "/");
        std::vector<std::string> dirs = string::split(path, "/");
        path = "";

        for (auto& d : dirs) {
            if (d.empty()) continue;

            if (path.empty()) {
                path = d;
            } else {
                path += "/" + d;
            }

            bool res = (::mkdir(path.c_str(), 0775) == 0) || (errno == EEXIST);

            if (!res) return false;
        }

        return true;
    }
}
