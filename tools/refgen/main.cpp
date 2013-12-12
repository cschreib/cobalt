#include <clang-c/Index.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <algorithm>
#include <filesystem.hpp>
#include <string.hpp>
#include <print.hpp>

template<typename T>
void get_location(T& t, CXSourceRange s) {
    std::ostringstream ss;
    CXFile f;
    unsigned int offset;
    clang_getSpellingLocation(clang_getRangeStart(s), &f, &t.lstart, &t.cend, &offset);
    clang_getSpellingLocation(clang_getRangeEnd(s), &f, &t.lend, &t.cstart, &offset);

    // For some reason, onle line declarations have start after end...
    if (t.lstart >= t.lend && t.cstart > t.cend) {
        std::swap(t.lstart, t.lend);
        std::swap(t.cstart, t.cend);
    } else {
        // For some reason, the starting character is always one too much
        t.cstart -= 1;
        // and the ending one is always one too few
        t.cend += 1;
    } 
}

bool is_in_file(CXCursor c, const CXFileUniqueID& fid) {
    CXSourceRange r = clang_getCursorExtent(c);

    CXFile cf;
    unsigned int line;
    unsigned int column;
    unsigned int offset;
    clang_getSpellingLocation(clang_getRangeStart(r), &cf, &line, &column, &offset);
    CXFileUniqueID id;
    clang_getFileUniqueID(cf, &id);

    for (std::size_t i = 0; i < 3; ++i) {
        if (id.data[i] != fid.data[i]) return false;
    }

    return true;
}

bool is_in_parent(CXCursor child, CXCursor parent) {
    CXSourceRange r1 = clang_getCursorExtent(parent);
    CXSourceRange r2 = clang_getCursorExtent(child);

    unsigned int s1, s2, e1, e2;

    CXFile f;
    unsigned int line;
    unsigned int column;
    clang_getSpellingLocation(clang_getRangeStart(r1), &f, &line, &column, &s1);
    clang_getSpellingLocation(clang_getRangeStart(r2), &f, &line, &column, &s2);
    clang_getSpellingLocation(clang_getRangeEnd(r1), &f, &line, &column, &e1);
    clang_getSpellingLocation(clang_getRangeEnd(r2), &f, &line, &column, &e2);

    return (s2 >= s1 && s2 < e1) && (e2 >= s1 && e2 < e1);
}

struct struct_t {
    std::string usr;
    std::string name;
    unsigned int lstart, lend;
    unsigned int cstart, cend;
    std::vector<std::string> members;

    bool operator < (const struct_t& s) const {
        return usr < s.usr;
    }
    bool operator < (const std::string& s) const {
        return usr < s;
    }
    bool operator == (const struct_t& s) const {
        return usr == s.usr;
    }
    bool operator == (const std::string& s) const {
        return usr == s;
    }
};

struct cpos_t {
    CXCursor cur;
    struct_t& str;
};

struct client_data_t {
    std::deque<struct_t> db;
    std::vector<cpos_t> cstack;
    CXFileUniqueID main_file_id;
};

CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    CXCursorKind cKind = clang_getCursorKind(cursor);

    client_data_t& data = *(client_data_t*)client_data;

    if (cKind == CXCursor_StructDecl || cKind == CXCursor_ClassDecl) {
        if (is_in_file(cursor, data.main_file_id)) {
            while (!data.cstack.empty() && !is_in_parent(cursor, data.cstack.back().cur)) {
                data.cstack.pop_back();
            }

            CXString usr = clang_getCursorUSR(cursor);
            if (std::find(data.db.begin(), data.db.end(), clang_getCString(usr)) == data.db.end()) {
                struct_t s;
                s.usr = clang_getCString(usr);
                CXType t = clang_getCursorType(cursor);
                CXString ts = clang_getTypeSpelling(t);
                s.name = clang_getCString(ts);
                get_location(s, clang_getCursorExtent(cursor));

                if (string::start_with(s.name, "message::") ||
                    string::start_with(s.name, "request::")) {
                    data.db.push_back(s);
                    data.cstack.push_back({cursor, data.db.back()});
                }

                clang_disposeString(ts);
                clang_disposeString(usr);
            } else {
                clang_disposeString(usr);

                return CXChildVisit_Continue;
            }
        } else {
            return CXChildVisit_Continue;
        }
    } else if (cKind == CXCursor_FieldDecl) {
        if (is_in_file(cursor, data.main_file_id)) {
            while (!data.cstack.empty() && !is_in_parent(cursor, data.cstack.back().cur)) {
                data.cstack.pop_back();
            }
            
            if (!data.cstack.empty()) {
                CXString nameString = clang_getCursorDisplayName(cursor);
                data.cstack.back().str.members.push_back(clang_getCString(nameString));
                clang_disposeString(nameString);
            }
        }
    } else if (cKind == CXCursor_VarDecl) {
        if (!data.db.empty() && is_in_file(cursor, data.main_file_id)) {
            struct_t s;
            get_location(s, clang_getCursorExtent(cursor));
            for (auto& st : data.db) {
                if (st.name.empty() && s.lstart == st.lend) {
                    CXString nameString = clang_getCursorDisplayName(cursor);
                    st.name = clang_getCString(nameString);
                    clang_disposeString(nameString);
                    break;
                }
            }
        }
    }

    return CXChildVisit_Recurse;
}

void parse(const std::string& dir, const std::string& filename, int argc, char* argv[]) {
    std::string file = dir+"/"+filename;
    std::string autogen = dir+"/autogen/packets/"+filename;

    if (file::exists(autogen) && !file::is_older(autogen, file)) return;

    print(file);

    CXIndex cidx = clang_createIndex(0, 0);
    CXTranslationUnit ctu = clang_parseTranslationUnit(
        cidx, file.c_str(), argv, argc, 0, 0, CXTranslationUnit_None
    );

    client_data_t data;

    if (ctu) {
        bool fail = false;
        std::size_t ndiag = clang_getNumDiagnostics(ctu);
        for (std::size_t i = 0; i < ndiag; ++i) {
            CXDiagnostic d = clang_getDiagnostic(ctu, i);
            CXDiagnosticSeverity sev = clang_getDiagnosticSeverity(d);
            if (sev == CXDiagnostic_Error || sev == CXDiagnostic_Fatal) {
                CXString str = clang_formatDiagnostic(d, clang_defaultDiagnosticDisplayOptions());
                print(clang_getCString(str));
                clang_disposeString(str);
                fail = true;
            }
        }

        if (!fail) {
            CXFile main_file = clang_getFile(ctu, file.c_str());
            clang_getFileUniqueID(main_file, &data.main_file_id);
            clang_visitChildren(clang_getTranslationUnitCursor(ctu), &visitor, &data);
        } else {
            warning("could not parse '", file, "'");
        }

        clang_disposeTranslationUnit(ctu);
    } else {
        warning("could not parse '", file, "'");
    }
    
    clang_disposeIndex(cidx);
    
    std::ofstream out(autogen);
    for (auto iter = data.db.begin(); iter != data.db.end(); ++iter) {
        auto vec = string::split(iter->name, "::");
        if (vec.empty()) continue;
        vec.pop_back();

        out << "// " << iter->name << ": " << iter->lstart << ":" << iter->cstart << " to "
            << iter->lend << ":" << iter->cend << "\n";
        for (auto& n : vec) {
            out << "namespace " << n << " { ";
        }
        out << "\n";
        out << "template<typename T> T& operator << (T& p, const " << iter->name << "& t) {";
        if (!iter->members.empty()) {
            out << "\n";
            for (auto& m : iter->members) {
                out << "    p << t." << m << ";\n";
            }
            out << "    return p;\n";
        } else {
            out << " return p; ";
        }
        out << "}\ntemplate<typename T> T& operator >> (T& p, " << iter->name << "& t) {";
        if (!iter->members.empty()) {
            out << "\n";
            for (auto& m : iter->members) {
                out << "    p >> t." << m << ";\n";
            }
            out << "    return p;\n";
        } else {
            out << " return p; ";
        }
        out << "}\n";
        for (auto& n : vec) {
            out << "}";
        }
        out << "\n\n";
    }
}

void parse_directory(const std::string& dir, int argc, char* argv[]) {
    std::vector<std::string> files = file::list_files(dir+"/*.hpp");
    for (auto& f : files) {
        parse(dir, f, argc, argv);
    }

    std::vector<std::string> dirs = file::list_directories(dir);
    for (auto& d : dirs) {
        if (d == "autogen") continue;
        parse_directory(dir+"/"+d, argc, argv);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "refgen: missing work folder" << std::endl;
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

    for (auto& d : dirs) {
        if (!file::mkdir(d+"/autogen/packets")) continue;
        parse_directory(d, argc-1, argv+1);
    }

    return 0;
}
