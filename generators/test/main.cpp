#include <iostream>
#include <fstream>

extern "C" bool generate_universe(const char* serialized_config, char** errmsg) {
    std::ofstream test("toto.reussi");
    return true;
}
