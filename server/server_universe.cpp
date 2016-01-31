#include "server_universe.hpp"
#include "server_universe_serialize.hpp"
#include "server_space_object.hpp"
#include "server_state_game.hpp"
#include <std_addon.hpp>
#include <string.hpp>
#include <filesystem.hpp>
#include <fstream>

namespace server {
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
        phdr >> buffer_->header.header;

        if (buffer_->header.header == v1::version_header) {
            phdr >> buffer_->header.depth >> buffer_->header.nobject;

            buffer_->objects.resize(buffer_->header.nobject);
            for (auto& so : buffer_->objects) {
                file >> so;
            }
        } else {
            throw request::server::game_load::failure{
                request::server::game_load::failure::reason::invalid_saved_game,
                "unsupported save file version '"+buffer_->header.header+"'"
            };
        }
    }

    // NB: this function must always use the latest format
    void universe_serializer::load_data_first_pass() {
        // Ugly but...
        switch (buffer_->header.depth) {
            case  0 : return;
            case  2 : universe_.space_ = space_universe::make< 2>(); break;
            case  3 : universe_.space_ = space_universe::make< 3>(); break;
            case  4 : universe_.space_ = space_universe::make< 4>(); break;
            case  5 : universe_.space_ = space_universe::make< 5>(); break;
            case  6 : universe_.space_ = space_universe::make< 6>(); break;
            case  7 : universe_.space_ = space_universe::make< 7>(); break;
            case  8 : universe_.space_ = space_universe::make< 8>(); break;
            case  9 : universe_.space_ = space_universe::make< 9>(); break;
            case 10 : universe_.space_ = space_universe::make<10>(); break;
            case 11 : universe_.space_ = space_universe::make<11>(); break;
            case 12 : universe_.space_ = space_universe::make<12>(); break;
            case 13 : universe_.space_ = space_universe::make<13>(); break;
            case 14 : universe_.space_ = space_universe::make<14>(); break;
            case 15 : universe_.space_ = space_universe::make<15>(); break;
            case 16 : universe_.space_ = space_universe::make<16>(); break;
            default :  throw request::server::game_load::failure{
                request::server::game_load::failure::reason::invalid_saved_game,
                "the depth of the universe must be comprised between 2 and 16"
            };
        }

        // Create objects
        for (auto& so : buffer_->objects) {
            std::uint32_t id;
            std::uint16_t type;
            space::vec_t pos;
            so >> id >> pos;

            space_cell* cell = universe_.space_->try_reach(pos);
            if (!cell) {
                throw request::server::game_load::failure{
                    request::server::game_load::failure::reason::invalid_saved_game,
                    "invalid position for object "+string::convert(id)+
                    " ("+string::convert(pos)+")"
                };
            }

            auto* factory = universe_.object_factory_.get_factory(type);
            if (!factory) {
                throw request::server::game_load::failure{
                    request::server::game_load::failure::reason::invalid_saved_game,
                    "invalid object type code for object "+string::convert(id)+
                    " ("+string::convert(type)+")"
                };
            }

            cell->fill(factory->deserialize(type, so));
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
