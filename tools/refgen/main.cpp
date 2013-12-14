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
#include <crc32.hpp>

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

struct packet {
    struct member {
        std::string name;
        std::string type;
        bool is_enum = false;
    };
    std::string usr;
    std::string name;
    std::string simple_name;
    std::string parent_name;
    std::vector<std::string> parents;
    packet* parent = nullptr;
    std::string file;
    unsigned int lstart, lend;
    unsigned int cstart, cend;
    std::vector<member> members;

    bool operator < (const packet& s) const {
        return usr < s.usr;
    }
    bool operator < (const std::string& s) const {
        return usr < s;
    }
    bool operator == (const packet& s) const {
        return usr == s.usr;
    }
    bool operator == (const std::string& s) const {
        return usr == s;
    }
};

struct cpos_t {
    CXCursor cur;
    packet& str;
};

struct client_data_t {
    CXTranslationUnit tu;
    std::deque<packet> db;
    std::vector<cpos_t> cstack;
    std::string main_file;
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
            std::string susr = clang_getCString(usr);
            if (std::find(data.db.begin(), data.db.end(), susr) == data.db.end()) {
                CXString ts = clang_getTypeSpelling(clang_getCursorType(cursor));
                std::string name = clang_getCString(ts);

                if (string::start_with(name, "message::") ||
                    string::start_with(name, "request::")) {
                    packet s;

                    s.name = name;
                    s.usr = susr;
                    get_location(s, clang_getCursorExtent(cursor));
                    s.file = data.main_file;

                    s.parents = string::split(s.name, "::");
                    s.simple_name = s.parents.back();
                    s.parents.pop_back();
                    s.parent_name = string::join(s.parents, "::");

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
                packet& p = data.cstack.back().str;
                packet::member m;

                CXString nameString = clang_getCursorDisplayName(cursor);
                m.name = clang_getCString(nameString);
                clang_disposeString(nameString);

                CXType t = clang_getCursorType(cursor);
                CXString ts = clang_getTypeSpelling(clang_getCursorType(cursor));
                m.type = clang_getCString(ts);
                clang_disposeString(ts);

                CXType ut = clang_getEnumDeclIntegerType(clang_getTypeDeclaration(t));
                m.is_enum = ut.kind != CXType_Invalid;

                // For some reason, enum type names are of the form 'enum XXX'
                // where 'XXX' is the unqualified name of the enum type. Fix that.
                if (m.is_enum && string::start_with(m.type, "enum ")) {
                    m.type = p.name + "::" + string::erase_begin(m.type, 5);
                }

                p.members.push_back(m);
            }
        }
    } else if (cKind == CXCursor_VarDecl) {
        if (!data.db.empty() && is_in_file(cursor, data.main_file_id)) {
            packet s;
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

void generate_code(std::ostream& out, const packet& p) {
    out << "// " << p.name << ": " << p.lstart << ":" << p.cstart << " to "
        << p.lend << ":" << p.cend << "\n";
    std::vector<std::string> nsp;
    const packet* pa = &p;
    while (pa) {
        if (pa->parent == nullptr) {
            nsp = pa->parents;
        }

        pa = pa->parent;
    }

    for (auto& n : nsp) {
        out << "namespace " << n << " { ";
    }
    out << "\n";
    out << "template<typename T> T& operator << (T& p, const " << p.name << "& t) {";
    if (!p.members.empty()) {
        out << "\n";
        for (auto& m : p.members) {
            if (m.is_enum) {
                out << "    p << static_cast<typename std::underlying_type<"
                    << m.type << ">::type>(t." << m.name << ");\n";
            } else {
                out << "    p << t." << m.name << ";\n";
            }
        }
        out << "    return p;\n";
    } else {
        out << " return p; ";
    }
    out << "}\ntemplate<typename T> T& operator >> (T& p, " << p.name << "& t) {";
    if (!p.members.empty()) {
        out << "\n";
        for (auto& m : p.members) {
            if (m.is_enum) {
                out << "    p >> reinterpret_cast<typename std::underlying_type<"
                    << m.type << ">::type&>(t." << m.name << ");\n";
            } else {
                out << "    p >> t." << m.name << ";\n";
            }
        }
        out << "    return p;\n";
    } else {
        out << " return p; ";
    }
    out << "}\n";
    for (auto& n : nsp) {
        out << "}";
    }
    out << "\n";

    if (p.parent == nullptr) {
        out << "namespace packet_impl { template<> struct packet_builder<" << p.name << "> {\n";
        if (!p.members.empty()) {
            out << "    template<";
            for (std::size_t i = 0; i < p.members.size(); ++i) {
                if (i != 0) out << ", ";
                out << "typename T" << i;
            }
            out << ">\n";
        }
        out << "    " << p.name << " operator () (";
        for (std::size_t i = 0; i < p.members.size(); ++i) {
            if (i != 0) out << ", ";
            out << "T" << i << "&& t" << i;
        }
        out << ") {\n";
        out << "        " << p.name << " p;\n";
        if (!p.members.empty()) {
            for (std::size_t i = 0; i < p.members.size(); ++i) {
                out << "        p." << p.members[i].name << " = t" << i << ";\n";
            }
        }
        out << "        return p;\n";
        out << "    }\n";
        out << "};}\n";
    }

    out << "\n";
}

bool parse(const std::string& dir, const std::string& filename, std::deque<packet>& db,
    int argc, char* argv[]) {

    std::string file = dir+"/"+filename;
    std::string autogen = dir+"/autogen/packets/"+filename;

    if (file::exists(autogen) && !file::is_older(autogen, file)) return true;

    print(file);

    CXIndex cidx = clang_createIndex(0, 0);
    CXTranslationUnit ctu = clang_parseTranslationUnit(
        cidx, file.c_str(), argv, argc, 0, 0, CXTranslationUnit_None
    );

    client_data_t data;
    data.tu = ctu;

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
            data.main_file = file;
            CXFile main_file = clang_getFile(ctu, file.c_str());
            clang_getFileUniqueID(main_file, &data.main_file_id);
            clang_visitChildren(clang_getTranslationUnitCursor(ctu), &visitor, &data);
        } else {
            error("could not parse '", file, "'");
            return false;
        }

        clang_disposeTranslationUnit(ctu);
    } else {
        error("could not parse '", file, "'");
        return false;
    }
    
    clang_disposeIndex(cidx);

    for (auto& p1 : data.db) {
        for (auto& p2 : data.db) {
            if (p1.parent_name == p2.name) {
                p1.parent = &p2;
                break;
            }
        }
    }
    
    std::ofstream out(autogen);
    if (!out.is_open()) {
        error("could not open output file");
        note(autogen);
        return false;
    }

    for (auto iter = data.db.begin(); iter != data.db.end(); ++iter) {
        generate_code(out, *iter);
    }

    db.insert(db.end(), data.db.begin(), data.db.end());

    return true;
}

bool parse_directory(const std::string& dir, std::deque<packet>& db, int argc, char* argv[]) {
    std::vector<std::string> files = file::list_files(dir+"/*.hpp");
    for (auto& f : files) {
        if (!parse(dir, f, db, argc, argv)) return false;
    }

    std::vector<std::string> dirs = file::list_directories(dir);
    for (auto& d : dirs) {
        if (d == "autogen") continue;
        if (!parse_directory(dir+"/"+d, db, argc, argv)) return false;
    }

    return true;
}

std::string get_position(const packet& p) {
    std::ostringstream ss;
    ss << p.file << ":" << p.lstart << ": " << p.name;
    return ss.str();
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
        dir+"/common-netcom/include", 
        dir+"/server/include",
        dir+"/client/include"
    };

    if (argc > 2 && std::string(argv[2]) == "clean") {
        for (auto& d : dirs) {
            file::remove(d+"/autogen/packets");
        }
        return 0;
    }

    std::deque<packet> db;
    for (auto& d : dirs) {
        if (!file::mkdir(d+"/autogen/packets")) {
            error("could not create 'autogen' directory");
            note(d+"/autogen/packets");
            return 1;
        }

        if (!parse_directory(d, db, argc-1, argv+1)) return 1;
    }

    // Check redundancy with CRC32
    std::vector<std::uint32_t> crcs;
    crcs.reserve(db.size());
    for (auto& p : db) {
        crcs.push_back(get_crc32(p.simple_name));
    }
    
    bool error = false;
    for (std::size_t i = 0;   i < db.size(); ++i) {
        if (db[i].parent != nullptr) continue;
        for (std::size_t j = i+1; j < db.size(); ++j) {
            if (db[j].parent == nullptr && crcs[i] == crcs[j]) {
                error = true;
                std::cout << "crc32 collision test: collision detected:" << std::endl;
                std::cout << "  " << get_position(db[i]) << std::endl;
                std::cout << "  " << get_position(db[j]) << std::endl;
            }
        }
    }

    if (!error) {
        std::cout << "crc32 collision test: no collision found" << std::endl;
    } else {
        return 1;
    }

    return 0;
}
