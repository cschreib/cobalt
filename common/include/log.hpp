#ifndef LOG_HPP
#define LOG_HPP

#include <string>
#include <fstream>
#include <iostream>
#include "time.hpp"
#include "config.hpp"

namespace color {
    enum color_value : char {
        black = 0, red, green, yellow, blue, magenta, cyan, white, normal = -1
    };

    extern struct reset_t {} reset;
    extern struct bold_t {} bold;

    struct set {
        color_value col_;
        bool bold_;

        explicit set(color_value col, bool bold = false) : col_(col), bold_(bold) {}
    };

    std::ostream& operator << (std::ostream& o, set s);
    std::ostream& operator << (std::ostream& o, reset_t s);
    std::ostream& operator << (std::ostream& o, bold_t s);
}

namespace logger_impl {
    void print_stamp(std::ostream& o);
    void print_stamp_colored(std::ostream& o);
}

template<typename O>
class logger_base {
    O    out_;
    bool stdout_ = false;
    bool nostamp_ = false;
    bool nocolor_ = false;

public :
    logger_base() : stdout_(true), nostamp_(true) {}

    explicit logger_base(const std::string& name, config::state& conf) {
        std::string file;
        if (conf.get_value("log."+name+".file", file, "") && !file.empty()) {
            bool append = true;
            conf.get_value("log."+name+".append", append, append);
            if (append) {
                out_.open(file, std::ios::app);
            } else {
                out_.open(file);
            }
        }

        conf.get_value("log."+name+".stdout", stdout_, stdout_);
        conf.get_value("log."+name+".nostamp", nostamp_, nostamp_);
    }

private :
    template<typename T>
    void print__(const T& t) {
        if (out_.is_open()) {
            out_ << t;
        }
        if (stdout_) {
            std::cout << t;
        }
    }

    template<typename ... Args>
    void print_(Args&& ... args) {
        int v[] = {(print__(std::forward<Args>(args)), 0)...};

        if (out_.is_open()) {
            out_ << std::endl;
        }
        if (stdout_) {
            std::cout << std::endl;
        }
    }

    void print_stamp_() {
        if (!nostamp_) {
            if (out_.is_open()) {
                logger_impl::print_stamp(out_);
            }
            if (stdout_) {
                if (!nocolor_) {
                    logger_impl::print_stamp_colored(std::cout);
                } else {
                    logger_impl::print_stamp(std::cout);
                }
            }
        }
    }

public :
    template<typename ... Args>
    void print(Args&&... args) {
        print_stamp_();
        print_(std::forward<Args>(args)...);
    }

    template<typename ... Args>
    void error(Args&& ... args) {
        print_stamp_();

        if (out_.is_open()) {
            out_ << "error: ";
        }

        if (stdout_) {
            if (!nocolor_) {
                std::cout << color::set(color::red, true);
            }

            std::cout << "error: ";

            if (!nocolor_) {
                std::cout << color::reset;
            }
        }

        print_(std::forward<Args>(args)...);
    }

    template<typename ... Args>
    void warning(Args&& ... args) {
        print_stamp_();

        if (out_.is_open()) {
            out_ << "warning: ";
        }

        if (stdout_) {
            if (!nocolor_) {
                std::cout << color::set(color::yellow, true);
            }

            std::cout << "warning: ";

            if (!nocolor_) {
                std::cout << color::reset;
            }
        }

        print_(std::forward<Args>(args)...);
    }

    template<typename ... Args>
    void note(Args&& ... args) {
        print_stamp_();

        if (out_.is_open()) {
            out_ << "note: ";
        }

        if (stdout_) {
            if (!nocolor_) {
                std::cout << color::set(color::blue, true);
            }

            std::cout << "note: ";

            if (!nocolor_) {
                std::cout << color::reset;
            }
        }

        print_(std::forward<Args>(args)...);
    }

    template<typename ... Args>
    void reason(Args&& ... args) {
        print_stamp_();

        if (out_.is_open()) {
            out_ << "reason: ";
        }

        if (stdout_) {
            if (!nocolor_) {
                std::cout << color::set(color::blue, true);
            }

            std::cout << "reason: ";

            if (!nocolor_) {
                std::cout << color::reset;
            }
        }

        print_(std::forward<Args>(args)...);
    }
};

using logger = logger_base<std::ofstream>;

extern logger cout;

#endif

