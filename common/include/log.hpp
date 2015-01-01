#ifndef LOG_HPP
#define LOG_HPP

#include <string>
#include <fstream>
#include <iostream>
#include "time.hpp"
#include "scoped_connection_pool.hpp"
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

class logger_base {
protected :
    bool color_ = false;
    bool stamp_ = false;

    virtual void print_(color::set s)     {}
    virtual void print_(color::reset_t s) {}
    virtual void print_(color::bold_t s)  {}

public :
    virtual ~logger_base() = 0;

    virtual bool is_open() const = 0;

    void print_stamp() {
        if (stamp_) {
            print(color::set(color::normal,true)); print("[");
            print(color::set(color::cyan,  true)); print(today_str("/"));
            print(color::set(color::normal,true)); print("|");
            print(color::set(color::green, true)); print(time_of_day_str(":"));
            print(color::set(color::normal,true)); print("] ");
            print(color::set(color::normal,false));
        }
    }

    virtual void print(const std::string& s) = 0;

    void print(color::set s) {
        if (color_) {
            print_(s);
        }
    }

    void print(color::reset_t s) {
        if (color_) {
            print_(s);
        }
    }

    void print(color::bold_t s) {
        if (color_) {
            print_(s);
        }
    }

    virtual void endl() = 0;
};

template<typename O>
class ostream_logger_base : public logger_base {
    O& out_;

protected :
    scoped_connection_pool pool_;

public :
    ~ostream_logger_base() override {}

    ostream_logger_base(O& o) : out_(o) {}

    void print(const std::string& t) override {
        out_ << t;
    }

    void print_(color::set s) override {
        out_ << s;
    }

    void print_(color::reset_t s) override {
        out_ << s;
    }

    void print_(color::bold_t s) override {
        out_ << s;
    }

    void endl() override {
        out_ << std::endl;
    }
};

template<typename O>
class ostream_logger : public ostream_logger_base<O> {
    O out_;

public :
    ~ostream_logger() override {}

    ostream_logger(config::state& conf, const std::string& name) :
        ostream_logger_base<O>(out_) {

        std::string file;
        if (conf.get_value("log."+name+".file", file, "") && !file.empty()) {
            bool append = true;
            conf.get_value("log."+name+".append", append, append);
            if (append) {
                out_.open(file, std::ios::app);
            } else {
                out_.open(file);
            }

            this->pool_ << conf.bind("log."+name+".color", this->color_);
            this->pool_ << conf.bind("log."+name+".stamp", this->stamp_);
        }
    }

    bool is_open() const override {
        return out_.is_open();
    }
};

class cout_logger : public ostream_logger_base<std::ostream> {
public :
    ~cout_logger() override {}

    cout_logger(config::state& conf) : ostream_logger_base<std::ostream>(std::cout) {
        pool_ << conf.bind("log.cout.color", color_);
        pool_ << conf.bind("log.cout.stamp", stamp_);
    }

    bool is_open() const override {
        return true;
    }
};

class logger {
    std::vector<std::unique_ptr<logger_base>> out_;

public :
    logger() = default;

private :
    template<typename T>
    void forward_print_(const T& t) {
        for (auto& o : out_) {
            if (o->is_open()) {
                o->print(t);
            }
        }
    }

    template<typename T>
    void print__(const T& t) {
        using std::to_string;
        forward_print_(to_string(t));
    }

    void print__(const std::string& str) {
        forward_print_(str);
    }

    void print__(const char* str) {
        forward_print_(std::string(str));
    }

    void print__(color::set s) {
        forward_print_(s);
    }

    void print__(color::reset_t s) {
        forward_print_(s);
    }

    void print__(color::bold_t s) {
        forward_print_(s);
    }

private :
    template<typename ... Args>
    void print_(Args&& ... args) {
        int v[] = {(print__(std::forward<Args>(args)), 0)...};
    }

    void print_stamp_() {
        for (auto& o : out_) {
            if (o->is_open()) {
                o->print_stamp();
            }
        }
    }

    void endl_() {
        for (auto& o : out_) {
            if (o->is_open()) {
                o->endl();
            }
        }
    }

public :
    template<typename T, typename ... Args>
    void add_output(Args&&... args) {
        out_.emplace_back(new T(std::forward<Args>(args)...));
    }

    template<typename ... Args>
    void print(Args&&... args) {
        print_stamp_();
        print_(std::forward<Args>(args)...);
        endl_();
    }

    template<typename ... Args>
    void print_header(color::color_value v, const std::string hdr, Args&&... args) {
        print_stamp_();
        print_(color::set(v, true));
        print_(hdr+": ");
        print_(color::reset);
        print_(std::forward<Args>(args)...);
        endl_();
    }

    template<typename ... Args>
    void error(Args&& ... args) {
        print_header(color::red, "error", std::forward<Args>(args)...);
    }

    template<typename ... Args>
    void warning(Args&& ... args) {
        print_header(color::yellow, "warning", std::forward<Args>(args)...);
    }

    template<typename ... Args>
    void note(Args&& ... args) {
        print_header(color::blue, "note", std::forward<Args>(args)...);
    }

    template<typename ... Args>
    void reason(Args&& ... args) {
        print_header(color::blue, "reason", std::forward<Args>(args)...);
    }
};

using file_logger = ostream_logger<std::ofstream>;

extern logger cout;

#endif

