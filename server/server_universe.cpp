#include "server_universe.hpp"
#include "server_universe_serialize.hpp"
#include "server_space_object.hpp"
#include "server_state_game.hpp"
#include <std_addon.hpp>
#include <string.hpp>
#include <filesystem.hpp>
#include <fstream>

namespace server {
    universe::universe() : object_factory_(std::make_unique<space_object_factory>()) {
        // Add object factories?
    }

    universe::~universe() {}

    bool universe::create_space(std::size_t size) {
        // Ugly but...
        switch (size) {
            case  0 : return true;
            case  2 : space_ = space_universe::make< 2>(); return true;
            case  3 : space_ = space_universe::make< 3>(); return true;
            case  4 : space_ = space_universe::make< 4>(); return true;
            case  5 : space_ = space_universe::make< 5>(); return true;
            case  6 : space_ = space_universe::make< 6>(); return true;
            case  7 : space_ = space_universe::make< 7>(); return true;
            case  8 : space_ = space_universe::make< 8>(); return true;
            case  9 : space_ = space_universe::make< 9>(); return true;
            case 10 : space_ = space_universe::make<10>(); return true;
            case 11 : space_ = space_universe::make<11>(); return true;
            case 12 : space_ = space_universe::make<12>(); return true;
            case 13 : space_ = space_universe::make<13>(); return true;
            case 14 : space_ = space_universe::make<14>(); return true;
            case 15 : space_ = space_universe::make<15>(); return true;
            case 16 : space_ = space_universe::make<16>(); return true;
            default : return false;
        }
    }

    /// Serialization

    static const std::string master_file_name = "universe.csf";

    struct universe_serializer_internal_buffer : v1::universe_serializer_internal_buffer {};

    std::unique_ptr<universe_serializer> universe::make_serializer() {
        return std::make_unique<universe_serializer>(*this);
    }

    universe_serializer::universe_serializer(universe& uni) :
        server::serializable("serializer_universe"), universe_(uni) {}

    universe_serializer::~universe_serializer() {}

    // NB: this function must always use the latest format
    void universe_serializer::save_data() {
        buffer_ = std::make_unique<universe_serializer_internal_buffer>();

        if (!universe_.space_) {
            return;
        }

        // Basic info
        buffer_->header.depth = universe_.space_->depth();

        // Now explore the whole space to find objects and serialize them
        // First find out how many objects we have to pre-allocate enough memory
        universe_.space_->for_each_cell([&](const space_object&) {
            ++buffer_->header.nobject;
        });

        buffer_->objects.reserve(buffer_->header.nobject);

        // Now serialize each object
        universe_.space_->for_each_cell([&](const space_object& obj) {
            buffer_->objects.push_back(serialized_packet{});
            auto& sp = buffer_->objects.back();
            sp << obj.id();
            if (obj.cell()) {
                sp << obj.cell()->get_coordinates();
            } else {
                sp << space::vec_t();
            }

            sp << obj.type();

            obj.serialize(sp);
        });
    }

    // NB: this function must always use the latest format
    void universe_serializer::serialize(const std::string& dir) const {
        if (!buffer_) {
            throw std::runtime_error("cannot serialize if save_data() has not been called");
        }

        // Write header
        std::ofstream file(dir+master_file_name, std::ios::binary);

        serialized_packet tp;
        tp << buffer_->header.header << buffer_->header.depth << buffer_->header.nobject;
        file << tp;

        // Write objects
        for (auto& so : buffer_->objects) {
            file << so;
        }
    }

    void universe_serializer::deserialize(const std::string& dir) {
        buffer_ = std::make_unique<universe_serializer_internal_buffer>();

        // Read file
        std::ifstream file(dir+master_file_name, std::ios::binary);

        serialized_packet phdr;
        file >> phdr;
        if (!(phdr >> buffer_->header.header)) {
            throw request::server::game_load::failure{
                request::server::game_load::failure::reason::invalid_saved_game,
                "could not read save format version"
            };
        }

        if (buffer_->header.header == v1::version_header) {
            if (!(phdr >> buffer_->header.depth)) {
                throw request::server::game_load::failure{
                    request::server::game_load::failure::reason::invalid_saved_game,
                    "could not read universe size"
                };
            };

            if (!(phdr >> buffer_->header.nobject)) {
                throw request::server::game_load::failure{
                    request::server::game_load::failure::reason::invalid_saved_game,
                    "could not read number of objects in universe"
                };
            };

            buffer_->objects.resize(buffer_->header.nobject);
            for (auto& so : buffer_->objects) {
                file >> so;
            }
        } else {
            throw request::server::game_load::failure{
                request::server::game_load::failure::reason::invalid_saved_game,
                "unsupported save file version"
            };
        }
    }

    // NB: this function must always use the latest format
    void universe_serializer::load_data_first_pass() {
        if (!universe_.create_space(buffer_->header.depth)) {
            throw request::server::game_load::failure{
                request::server::game_load::failure::reason::invalid_saved_game,
                "the depth of the universe must be comprised between 2 and 16"
            };
        }

        // Create objects
        std::size_t i = 0;
        for (auto& so : buffer_->objects) {
            ++i;
            uuid_t id;
            space::vec_t pos;
            std::uint16_t type;

            if (!(so >> id >> pos >> type)) {
                throw request::server::game_load::failure{
                    request::server::game_load::failure::reason::invalid_saved_game,
                    "could not read generic object properties for object "+
                    string::convert(i)+"/"+string::convert(buffer_->objects.size())
                };
            }

            space_cell* cell = universe_.space_->try_reach(pos);
            if (!cell) {
                throw request::server::game_load::failure{
                    request::server::game_load::failure::reason::invalid_saved_game,
                    "invalid position for object "+string::convert(id)+
                    " ("+string::convert(pos)+"), out of universe"
                };
            } else if (!cell->empty()) {
                throw request::server::game_load::failure{
                    request::server::game_load::failure::reason::invalid_saved_game,
                    "invalid position for object "+string::convert(id)+
                    " ("+string::convert(pos)+"), object "+
                    string::convert(cell->content().id())+" is already there"
                };
            }

            std::unique_ptr<space_object> ptr = universe_.object_factory_->make(type, id);
            if (!ptr) {
                throw request::server::game_load::failure{
                    request::server::game_load::failure::reason::invalid_saved_game,
                    "invalid object type code for object "+string::convert(id)+
                    " ("+string::convert(type)+")"
                };
            }

            ptr->deserialize(so);
            cell->fill(std::move(ptr));
        }
    }

    // NB: this function must always use the latest format
    void universe_serializer::load_data_second_pass() {};


    void universe_serializer::clear() {
        buffer_ = nullptr;
    };

    bool universe_serializer::is_valid_directory(const std::string& dir) const {
        return file::exists(dir+master_file_name);
    }
}
