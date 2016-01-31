#include <iostream>
#include <fstream>
#include "generator_base.hpp"
#include "generator_base_internal.hpp"

void generate_universe_impl(const std::string& out_dir, const config::state& conf) {
    std::size_t udepth;
    if (!conf.get_value("universe.size", udepth)) {
        throw std::runtime_error("missing size of the universe ('universe.size')");
    }

    auto universe = std::make_unique<server::universe>();
    universe->create_space(udepth);

    auto universe_serializer = universe->make_serializer();
    universe_serializer->save_data();
    universe_serializer->serialize(out_dir);
}
