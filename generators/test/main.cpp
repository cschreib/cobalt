#include <iostream>
#include <fstream>
#include "generator_base.hpp"

extern "C" bool generate_universe(const char* serialized_config, char** errmsg) {
    std::ofstream test("toto.reussi");
    return true;
}
