#include "log.hpp"

logger cout;

namespace color {
    reset_t reset;
    bold_t bold;
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
        std::ostream& operator << (std::ostream& o, reset_t) {
            return o << "\033[0m";
        }

        std::ostream& operator << (std::ostream& o, bold_t) {
            return o << "\033[1m";
        }

        std::ostream& operator << (std::ostream& o, set s) {
            if (s.col_ == color::normal) {
                o << reset;
                if (s.bold_) o << bold;
                return o;
            } else {
                return o << color_codes[s.bold_ ? 1 : 0][(char)s.col_ & 7];
            }
        }
    }

// Fallback: no terminal color
#else

    namespace color {
        std::ostream& operator << (std::ostream& o, reset_t) {
            return o;
        }

        std::ostream& operator << (std::ostream& o, bold_t) {
            return o;
        }

        std::ostream& operator << (std::ostream& o, set s) {
            return o;
        }
    }

#endif

logger_base::~logger_base() {}
