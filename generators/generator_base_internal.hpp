#ifndef GENERATOR_BASE_INTERNAL_HPP
#define GENERATOR_BASE_INTERNAL_HPP

#include <string.hpp>
#include <config.hpp>
#include <server_universe.hpp>
#include <cstring>

char* make_error(const std::string& s) {
    char* errmsg = new char[s.size()+1];
    strcpy(errmsg, s.data());
    return errmsg;
}

// Actual implementation
void generate_universe_impl(const std::string&, const config::state&);

extern "C" bool generate_universe(const char* serialized_config, char** errmsg) {
    std::istringstream ss(serialized_config);
    config::state conf;
    conf.parse(ss);

    try {
        std::string out_dir;
        conf.get_value("output_directory", out_dir);
        generate_universe_impl(out_dir, conf);
    } catch (std::exception& e) {
        *errmsg = make_error(e.what());
        return false;
    } catch (...) {
        *errmsg = make_error("unexpected error");
        return false;
    }

    return true;
}

extern "C" void free_error(char* errmsg) {
    delete[] errmsg;
}


#endif
