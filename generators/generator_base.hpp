#ifndef GENERATOR_BASE_HPP
#define GENERATOR_BASE_HPP

extern "C" void free_error(char* errmsg) {
    delete[] errmsg;
}

#endif
