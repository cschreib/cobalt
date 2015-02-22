#ifndef SERVER_SERIALIZABLE_HPP
#define SERVER_SERIALIZABLE_HPP

#include <string>

namespace server {
    class serializable {
        std::string name_;

    public :
        serializable(std::string name);
        virtual ~serializable() = default;

        // Simple localizable name describing the serializable data
        const std::string& name() const;

        // Store current game state into cached serializable structs
        virtual void save_data() = 0;
        // Serialize the serializable structs on disk
        virtual void serialize(const std::string& dir) const = 0;

        // De-serizalize saved content into serializable structs
        virtual void deserialize(const std::string& dir) = 0;
        // Load data from serializable structs into current game state
        virtual void load_data_firt_pass() = 0;
        virtual void load_data_second_pass() {};

        // Check that the given directory contains the necessary data
        virtual bool is_valid_directory(const std::string& dir) const = 0;
    };
}

#endif
