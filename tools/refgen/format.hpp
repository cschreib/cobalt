std::ostream& out = std::cerr;

namespace color {
    enum color_value : char {
        black = 0, red, green, yellow, blue, magenta, cyan, white, normal = -1
    };

    std::ostream& reset(std::ostream& o);
    std::ostream& bold(std::ostream& o);

    struct set {
        color_value col_;
        bool bold_;

        explicit set(color_value col, bool bold = false) : col_(col), bold_(bold) {}
    };

    std::ostream& operator << (std::ostream& o, set s);
}

// Use POSIX terminal colors
#ifdef POSIX
    #include <unistd.h>
    #include <sys/ioctl.h>

    // Note: code for coloring the terminal is inspired from LLVM
    #define COLOR(FGBG, CODE, BOLD) "\033[0;" BOLD FGBG CODE "m"

    #define ALLCOLORS(FGBG,BOLD) {\
    COLOR(FGBG, "0", BOLD),\
    COLOR(FGBG, "1", BOLD),\
    COLOR(FGBG, "2", BOLD),\
    COLOR(FGBG, "3", BOLD),\
    COLOR(FGBG, "4", BOLD),\
    COLOR(FGBG, "5", BOLD),\
    COLOR(FGBG, "6", BOLD),\
    COLOR(FGBG, "7", BOLD)\
    }

    static const char color_codes[2][8][10] = {
        ALLCOLORS("3",""), ALLCOLORS("3","1;")
    };

    #undef COLOR
    #undef ALLCOLORS

    namespace color {
        std::ostream& reset(std::ostream& o) {
            return o << "\033[0m";
        }

        std::ostream& bold(std::ostream& o) {
            return o << "\033[1m";
        }

        std::ostream& operator << (std::ostream& o, set s) {
            if (s.col_ == color::normal) {
                reset(o);
                if (s.bold_) bold(o);
                return o;
            } else {
                return o << color_codes[s.bold_ ? 1 : 0][(char)s.col_ & 7];
            }
        }
    }

// Fallback: no terminal color
#else

    namespace color {
        std::ostream& reset(std::ostream& o) {
            return o;
        }

        std::ostream& bold(std::ostream& o) {
            return o;
        }

        std::ostream& operator << (std::ostream& o, set s) {
            return o;
        }
    }

#endif

bool file_is_same(CXFile f1, CXFile f2) {
    CXFileUniqueID id1, id2;
    clang_getFileUniqueID(f1, &id1);
    clang_getFileUniqueID(f2, &id2);

    for (std::size_t i = 0; i < 3; ++i) {
        if (id1.data[i] != id2.data[i]) return false;
    }

    return true;
}

std::string location_str(CXSourceLocation s) {
    std::ostringstream ss;
    CXFile f;
    unsigned int line;
    unsigned int column;
    unsigned int offset;
    clang_getSpellingLocation(s, &f, &line, &column, &offset);
    CXString fname = clang_getFileName(f);
    ss << clang_getCString(fname) << ":" << line << ":" << column;
    clang_disposeString(fname);

    return ss.str();
}

std::size_t terminal_width() {
    #ifdef POSIX
        struct winsize w;
        ioctl(STDERR_FILENO, TIOCGWINSZ, &w);
        return w.ws_col;
    #else
        return 80;
    #endif
}

template<typename CharType>
bool is_any_of(CharType c, std::basic_string<CharType> chars) {
    return chars.find(c) != std::basic_string<CharType>::npos;
}

template<typename CharType>
std::basic_string<CharType> string_range(std::basic_string<CharType> str, std::size_t b, std::size_t e) {
    using string = std::basic_string<CharType>;
    if (e == string::npos) {
        return str.substr(b);
    } else {
        return str.substr(b, e-b);
    }
}

template<typename CharType>
std::basic_string<CharType> wrap(std::basic_string<CharType> msg,
    std::size_t head, std::size_t indent, std::size_t maxwidth) {
    using string = std::basic_string<CharType>;

    if (indent >= maxwidth) return "...";

    string res;
    bool first = true;

    if (head >= maxwidth) {
        // The header is too large already, no text will be written on the first line.
        first = false;
    }

    const string spaces = " \t";

    std::size_t line_begin = msg.find_first_not_of(spaces);
    std::size_t line_end = line_begin;

    while (line_begin + maxwidth - (first ? head : indent) < msg.size()) {
        std::size_t pos = line_begin + maxwidth - (first ? head : indent);
        auto c = msg[pos];
        std::size_t new_begin;
        if (is_any_of(c, spaces)) {
            // Clipping occurs in the middle of a empty region.
            // Look for the end of the previous word.
            line_end = msg.find_last_not_of(spaces, pos);
            if (line_end == string::npos) {
                // No word found, discard this line.
                line_begin = line_end;
            } else {
                // Word found, keep it on this line.
                ++line_end;
            }

            // Set the begining of the new line to the next word.
            new_begin = msg.find_first_not_of(spaces, pos);
        } else {
            // Clipping occurs in the middle of a word.
            // Look for the begining of this word.
            line_end = msg.find_last_of(spaces, pos);
            if (line_end == string::npos) {
                if (first) {
                    // The header is too large, just start the message on the next line
                    new_begin = line_begin;
                    line_begin = string::npos;
                } else {
                    // This is the only word for this line, we have no choice but to keep it,
                    // even if it is too long.
                    line_end = msg.find_first_of(spaces, pos);
                    new_begin = msg.find_first_not_of(spaces, line_end);
                }
            } else {
                // Keep this word as the beginning of the next line.
                ++line_end;
                new_begin = line_end;
            }
        }

        if (line_begin != string::npos) {
            if (!first) res += '\n'+string(indent, ' ');
            res += string_range(msg, line_begin, line_end);
        }

        line_begin = new_begin;
        first = false;

        if (line_begin == string::npos) {
            // No word remaining.
            break;
        }
    }

    if (line_begin != string::npos) {
        // There are some words remaining, put them on the last line.
        line_end = msg.find_last_not_of(spaces);
        if (line_end != string::npos) {
            ++line_end;

            if (!first) res += '\n'+string(indent, ' ');
            res += string_range(msg, line_begin, line_end);
        }
    }

    return res;
}

void format_diagnostic(CXDiagnostic d);

void format_diagnostics(CXDiagnosticSet ds) {
    for (std::size_t i = 0; i < clang_getNumDiagnosticsInSet(ds); ++i) {
        CXDiagnostic d = clang_getDiagnosticInSet(ds, i);
        format_diagnostic(d);
        clang_disposeDiagnostic(d);
    }
}

void format_range(CXDiagnostic d, CXSourceLocation sl) {
    CXFile floc;
    unsigned int lloc, cloc; {
        unsigned int offset;
        clang_getSpellingLocation(sl, &floc, &lloc, &cloc, &offset);
    }

    std::string filename; {
        CXString tmp = clang_getFileName(floc);
        filename = clang_getCString(tmp);
        clang_disposeString(tmp);
    }

    std::ifstream fs(filename);
    if (!fs.is_open()) return;

    std::string line;
    for (std::size_t i = 0; i < lloc; ++i) {
        std::getline(fs, line);
    }

    std::size_t coffset = 0;
    std::size_t width = line.size();
    std::size_t max_width = terminal_width();
    if (width > max_width) {
        // The line is too long to fit on the terminal.
        // Just truncate it for now (TODO: improve that?)
        if (max_width >= 3) {
            line.erase(max_width-3);
            line += "...";
        } else {
            line.erase(max_width);
        }

        width = max_width;
    }

    out << line << '\n';

    std::string highlight = std::string(width, ' ');

    std::size_t nrange = clang_getDiagnosticNumRanges(d);
    for (std::size_t i = 0; i < nrange; ++i) {
        CXSourceRange r = clang_getDiagnosticRange(d, i);

        CXFile fstart, fend;
        unsigned int lstart, lend, cstart, cend; {
            unsigned int offset;
            clang_getSpellingLocation(clang_getRangeStart(r), &fstart, &lstart, &cstart, &offset);
            clang_getSpellingLocation(clang_getRangeEnd(r), &fend, &lend, &cend, &offset);
        }

        if (cend < cstart) std::swap(cend, cstart);

        if (file_is_same(floc, fstart) && lloc == lstart &&
            cstart >= coffset && cend <= coffset+width) {
            for (std::size_t j = cstart; j < cend; ++j) {
                highlight[j-1 - coffset] = '~';
            }
        }
    }

    if (cloc-1 >= coffset && cloc-1 < coffset + width) {
        highlight[cloc-1 - coffset] = '^';
    }

    out << color::set(color::green, true) << highlight << color::reset << '\n';
}

void format_diagnostic(CXDiagnostic d) {
    auto col = color::normal;
    std::string kind = "";
    bool bold_message = true;

    switch (clang_getDiagnosticSeverity(d)) {
    case CXDiagnostic_Note :
        col = color::black;
        kind = "note: ";
        bold_message = false;
        break;
    case CXDiagnostic_Warning :
        col = color::magenta;
        kind = "warning: ";
        break;
    case CXDiagnostic_Error :
    case CXDiagnostic_Fatal :
        col = color::red;
        kind = "error: ";
        break;
    case CXDiagnostic_Ignored :
    default : return;
    }

    CXSourceLocation sl = clang_getDiagnosticLocation(d);

    std::string loc;
    if (clang_equalLocations(sl, clang_getNullLocation()) == 0) {
        loc = location_str(sl)+": ";
    }

    std::string message; {
        CXString tmp = clang_getDiagnosticSpelling(d);
        message = wrap(std::string(clang_getCString(tmp)),
            loc.size()+kind.size(), 6, terminal_width()
        );
        clang_disposeString(tmp);
    }

    out << color::set(color::normal, true) << loc
        << color::set(col, true) << kind
        << color::set(color::normal, bold_message) << message
        << color::reset << "\n";

    if (!loc.empty()) {
        format_range(d, sl);
    }

    format_diagnostics(clang_getChildDiagnostics(d));
    out << std::flush;
}
