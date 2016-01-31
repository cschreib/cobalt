#ifndef GENERATOR_BASE_HPP
#define GENERATOR_BASE_HPP

extern "C" bool generate_universe(const char* serialized_config, char** errmsg);
extern "C" void free_error(char* errmsg);

#endif
