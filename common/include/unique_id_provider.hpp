#ifndef UNIQUE_ID_PROVIDER_HPP
#define UNIQUE_ID_PROVIDER_HPP

#include "sorted_vector.hpp"

namespace ctl {
    template<typename T>
    class unique_id_provider {
        ctl::sorted_vector<T, std::greater<T>> ids_;
        T max_id_;
        T first_;

    public :
        explicit unique_id_provider(T max_id = std::numeric_limits<T>::max(), T first = 0) :
            max_id_(max_id), first_(first) {
            ids_.reserve(max_id);
            clear();
        }

        bool make_id(T& id) {
            if (ids_.empty()) return false;
            id = ids_.back();
            ids_.pop_back();
            return true;
        }

        void free_id(T id) {
            if (id < max_id_) {
                ids_.insert(id);
            }
        }

        void clear() {
            ids_.clear();
            for (std::size_t i = 0; i < max_id_; ++i) {
                ids_.insert(max_id_ - i - 1 + first_);
            }
        }

        bool empty() const {
            return ids_.empty();
        }

        void set_max_id(T max_id) {
            if (max_id_ == max_id) return;

            if (max_id_ < max_id) {
                // Allocate new ids
                ids_.reserve(max_id);
                for (std::size_t i = max_id_; i < std::size_t(max_id); ++i) {
                    ids_.insert(max_id - i - 1 + first_);
                }
            } else {
                // Remove ids that are no longer accessible
                if (max_id == 0) {
                    ids_.clear();
                } else {
                    for (std::size_t i = max_id_ - 1; i >= std::size_t(max_id); --i) {
                        ids_.erase(max_id - i - 1 + first_);
                    }
                }
            }

            max_id_ = max_id;
        }
    };
}

#endif
