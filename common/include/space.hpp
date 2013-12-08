#ifndef SPACE_HPP
#define SPACE_HPP

#include "axis_aligned_box2.hpp"
#include "sorted_vector.hpp"
#include <memory>
#include <stdexcept>

namespace space {
    /// Underlying type of a coordinate in the universe
    using pos_t = std::int32_t;
    /// Underlying type of a position in the universe
    using vec_t = vec2_t<pos_t>;
    /// List of all possible directions one can move in
    enum class direction { LEFT = 0, UP, RIGHT, DOWN };

    /// Base exception class that encompasses all the 'space' exceptions.
    struct exception : public std::runtime_error {
        explicit exception(const std::string& s);
        explicit exception(const char* s);
        virtual ~exception() noexcept;
    };

    /// Exception raised when trying to put an object in a cell that
    /// already contains one.
    struct cell_occupied_exception : public exception {
        cell_occupied_exception();
    };

    /// Exception raised when trying to probe a direction that does not exist.
    struct invalid_direction_exception : public exception {
        invalid_direction_exception();
    };

    /// Exception raised when trying to access a position that does not
    /// belong to the universe.
    struct invalid_position_exception : public exception {
        explicit invalid_position_exception(const vec_t& pos);
    };

    template<typename T>
    class cell;

    namespace impl {
        /// Underlying type of an internal coordinate.
        using pos_t = std::make_unsigned<space::pos_t>::type;
        /// Underlying type of an internal position.
        using vec_t = vec2_t<pos_t>;

        template<typename T, std::size_t D>
        struct universe;
    }

    /// The unit cell of the universe.
    /** It is not splitable, and is the only type of cell that can
        contain actual objects. The only features it offers are:

         * containing an object:
           content, fill, release, clear, empty
         * accessing the neighboring cells:
           reach, try_reach
         * computing its asolute coordinates:
           get_coordinates

        The other purpose of this class is to do type erasure:
        the code that uses this set of classes does not need to
        know the actual depth of the universe, nor what the actual
        cell type is. It should only interact through this provided
        interface. See space::universe for another example.
    **/
    template<typename T>
    class cell {
    public :
        virtual ~cell() {}

        /// Return a vector containing the absolute position of this cell.
        virtual vec_t get_coordinates() const = 0;

        /// Return the neighboring cell in the direction 'dir'.
        /** \param dir The direction in which to look for the neighboring cell
            \note Will return nullptr if this direction goes past the boundaries of the
                  universe.
            \note If the space was not subdivided here, a new cell is created.
                  Hence there is no const version of this function. To explore
                  the neighboring cells without affecting the structure of space,
                  use try_reach.
        **/
        virtual cell* reach(direction dir) = 0;

        /// Try to reach the neighboring cell in the direction 'dir'.
        /** \param dir The direction in which to look for the neighboring cell
            \note Will return nullptr if no cell exists in this direction, either
                  because the boundaries of the universe have been reached, or
                  because the space is not subdivided here.
        **/
        cell* try_reach(direction dir) {
            return const_cast<cell*>(const_cast<const cell&>(*this).try_reach(dir));
        }

        /// Try to reach the neighboring cell in the direction 'dir'.
        /** \param dir The direction in which to look for the neighboring cell
            \note Will return nullptr if no cell exists in this direction, either
                  because the boundaries of the universe have been reached, or
                  because the space is not subdivided here.
        **/
        virtual const cell* try_reach(direction dir) const = 0;

        /// Return the content of this cell.
        /** \note You must first ensure that this cell actually contains anything
                  by evalutating the result of empty().
        **/
        T& content() {
            return *obj_;
        }

        /// Return the content of this cell.
        /** \note You must first ensure that this cell actually contains anything
                  by evalutating the result of empty().
        **/
        const T& content() const {
            return *obj_;
        }

        /// Sets the content of this cell.
        /** \param t The new content of this cell
            \note An exception is thrown if this cell already contains an object.
                  See cell_occupied_exception.
            \note May not be called with a nullptr object. Use clear() to destroy
                  the contained object.
            \note If T provides an accessible member function named 'notify_parent_cell'
                  that takes a naked pointer to a space::cell<T> as its first and only
                  argument, then this function will call it to notify T of its new
                  parent. If you declare this function as private, do not forget to
                  befriend space::cell<T>.
        **/
        T& fill(std::unique_ptr<T>&& t) {
            if (obj_) throw space::cell_occupied_exception();

            obj_ = std::move(t);
            obj_notify_parent_cell_(this, has_notify_parent_cell());

            return *obj_;
        }

        /// Move the content of this cell into another one.
        /** \param c The cell into which to move the content of this cell
            \note An exception is thrown if the other cell already contains an object.
                  See cell_occupied_exception.
            \note If this cell doens't contain anything, then this function does nothing.
            \note If T provides an accessible member function named 'notify_parent_cell'
                  that takes a naked pointer to a space::cell<T> as its first and only
                  argument, then this function will call it to notify T of its new
                  parent. If you declare this function as private, do not forget to
                  befriend space::cell<T>.
        **/
        void move_content_to(cell& c) {
            if (!obj_) return;
            if (c.obj_) throw space::cell_occupied_exception();

            c.obj_ = std::move(obj_);
            obj_notify_parent_cell_(&c, has_notify_parent_cell());
        }

        /// Release the contained object so it can be moved elsewhere.
        /** \note After this call, this cell becomes empty and the
                  object becomes owned by the caller.
            \note If T provides an accessible member function named 'notify_parent_cell'
                  that takes a reference to a space::cell<T> as its first and only
                  argument, then this function will call it to notify T that it no
                  longer has any parent. If you declare this function as private, do not
                  forget to befriend space::cell<T>.
        **/
        std::unique_ptr<T>&& release() {
            obj_notify_parent_cell_(nullptr, has_notify_parent_cell());
            return std::move(obj_);
        }

        /// Destroy the contained object.
        /** \param nodelete If the parent of this cell does not contain any occupied
                            cell after this call, it will delete all its child cells
                            to release memory. Set this value to 'false' to prevent it.
        **/
        void clear(bool nodelete = false) {
            obj_ = nullptr;
            notify_empty_();
        }

        /// Check if this cell contains an object.
        bool empty() const {
            return obj_ == nullptr;
        }

    protected :
        cell() = default;
        cell(const cell&) = delete;
        cell& operator=(const cell&) = delete;
        cell(cell&&) = default;
        cell& operator=(cell&&) = default;

        /// Reimplemented by the actual cell type to notify its parent.
        virtual void notify_empty_() {}

    private :
        // Tools to check if T has a 'notify_parent_cell' member function
        // and call it only if that is the case.
        struct has_notify_parent_cell_t {
            template <typename U> static std::true_type  dummy(decltype(std::declval<U>().notify_parent_cell(std::declval<space::cell<U>&>()))*);
            template <typename U> static std::false_type dummy(...);
            using type = decltype(dummy<T>(nullptr));
        };

        using has_notify_parent_cell = typename has_notify_parent_cell_t::type;

        void obj_notify_parent_cell_(cell* c, std::true_type) {
            obj_->notify_parent_cell(*c);
        }

        void obj_notify_parent_cell_(cell* c, std::false_type) {}

    private :
        /// The contained object, if any.
        std::unique_ptr<T> obj_;
    };

    /// Contains all the space cells.
    /** This class is the base class of space management.

        The universe is created as a square that is subdivided
        into four sub-squares called cells, which are themselves
        subdivided into four smaller sub-squares, and so on and so
        forth. This is called a quad-tree. In this context, D is the
        maximum number of times the universe is subdivided (1: a
        single cell, 2: four cells, ...). The total number of unit
        cells in the universe is 4^(D-1). To save some memory, only
        the cells that actually contain an object are created.

        D is specified once, when the universe is created. See make().
        Note that, for implementation purposes, D should not be
        greater than 33 in 32 bits environment, and 65 in 64 bits
        ones (in general, less or equal to N+1 in N bits environment).
        If you need go past this limit, then you will need to edit
        space::pos_t and space::impl::pos_t to be larger integers
        that can take values at least up to 2^(D-1).
        Also keep in mind that, although there is some memory
        optimization:

         * Each cell can be assumed to weight 2*W bytes, where W
           is the size of a machine word (i.e. 4 bytes for 32 bits,
           8 bytes for 64bits).
         * The total number of cells in a universe that is fully
           resolved (i.e. where all cells contain an object) is
           (4^D - 1)/3. Assuming the above statement, and
           considering that the maximum available memory in
           any machine is 2^(8*W), this translates into the
           theoretical limit: D < log2(3*2^(8*W-1)/W + 1)/2.
           This yields 15 for 32 bits, and 30 for 64 bits.
           This is theoretical in the sense that it assumes that
           all the memory is available, which is never the case,
           that all cells are occupied, which should not happen
           (else it is better to use a plain array), and that
           the contained objects do not occupy memory.
         * When N unit cells are created, the memory overhead
           that is generated by their parent cells is, at best,
           33% (compared to an ideal but impractical case where
           only the N unit cells are stored in memory).

        Any cell in this structure can be reached in O(D).
    **/
    template<typename T>
    class universe {
    public :
        /// Create a new universe of depth D.
        template<std::size_t D>
        static std::unique_ptr<universe> make() {
            return std::unique_ptr<universe>(new impl::universe<T,D>());
        }

        /// Return the number of cells that this universe contains in each dimension.
        /** This is actually 2^(D-1).
        **/
        virtual std::size_t size() const = 0;

        /// Return the internal depth of this universe.
        /** This is D.
        **/
        virtual std::size_t depth() const = 0;

        /// Pick the cell that lies at the given position.
        /** \note Will create this cell if it doesn't exist. If you want to
                  navigate the space without affecting its structure, use
                  try_reach().
            \note Will throw an exception if this position references a place
                  that lies outside of this universe's boundaries.
            \note For a universe of depth D, the range of reachable coordinates
                  is [-2^(D-2), 2^(D-2) - 1].
        **/
        virtual cell<T>& reach(const vec_t& pos) = 0;

        /// Pick the cell that lies at the given position.
        /** \note Will not create this cell if it doesn't exist, and will
                  return nullptr instead.
            \note Will return nullptr if this position references a place
                  that lies outside of this universe's boundaries.
            \note For a universe of depth D, the range of reachable coordinates
                  is [-2^(D-2), 2^(D-2) - 1].
        **/
        cell<T>* try_reach(const vec_t& pos) {
            return const_cast<cell<T>*>(const_cast<const universe&>(*this).try_reach(pos));
        }

        /// Pick the cell that lies at the given position.
        /** \note Will not create this cell if it doesn't exist, and will
                  return nullptr instead.
            \note Will return nullptr if this position references a place
                  that lies outside of this universe's boundaries.
            \note For a universe of depth D, the range of reachable coordinates
                  is [-2^(D-2), 2^(D-2) - 1].
        **/
        virtual const cell<T>* try_reach(const vec_t& pos) const = 0;

        /// Selects all the cells that lie inside the provided bounding box.
        /** \param box  The box in which to select the cells. The coordinates
                        must reference individual cells, much like the positions
                        in reach() and try_reach().
            \param list The container in which to store the selected cells. Note
                        that it can be efficient to estimate the expected number
                        of cells that will be selected and reserve enough memory
                        for them beforehand. This function will not take care
                        about it.
        **/
        virtual void clip(const axis_aligned_box2d& box, sorted_vector<const cell<T>&>& list) const = 0;

    protected :
        universe() = default;
        universe(const universe&) = delete;
        universe& operator=(const universe&) = delete;
        universe(universe&&) = default;
        universe& operator=(universe&&) = default;
    };

    // Implementation classes, should not be used directly.
    namespace impl {
        /// List of all the sub-cells that form a split cell.
        struct sub_cell {
        static const std::size_t TL = 0, TR = 1, BL = 2, BR = 3;
        };

        template<typename T, std::size_t D>
        struct universe;

        template<typename T, std::size_t N, std::size_t D>
        struct any_cell;

        /// Contains four other cells.
        /** The only purpose of this class is to contain four cells,
            nothing more. They are allocated contiguously in memory.
        **/
        template<typename T, std::size_t N, std::size_t D>
        struct split_cell {
            any_cell<T,N+1,D> childs[4];

        private :
            friend any_cell<T,N,D>;

            explicit split_cell(any_cell<T,N,D>& self) : childs({
                any_cell<T,N+1,D>(self), any_cell<T,N+1,D>(self), any_cell<T,N+1,D>(self), any_cell<T,N+1,D>(self)
            }) {}

            split_cell(const split_cell&) = delete;
            split_cell& operator=(const split_cell&) = delete;
            split_cell(split_cell&&) = default;
            split_cell& operator=(split_cell&&) = default;
        };

        /// Return true if the requested cell lies within this split cell,
        /// false if it lies in a neighboring cell.
        /// Set 'next_id' to the id of the requested cell in its parent's
        /// child list.
        bool get_cell_id(std::size_t id, direction dir, std::size_t& next_id);

        /// A cell of the space quad-tree, can either be empty or split.
        template<typename T, std::size_t N, std::size_t D>
        struct any_cell {
            friend any_cell<T,N+1,D>;
            friend split_cell<T,N-1,D>;

            /// Check if this cell is empty.
            /** I.e. if it is not split.
            **/
            bool empty() const { return split == nullptr; }

            /// Split this cell into several subcells to refine the sampling of space.
            void make_split() { split.reset(new split_cell<T,N,D>(*this)); }

            /// The parent of this cell.
            any_cell<T,N-1,D>& parent;

            /// The split sub-cells, if any.
            std::unique_ptr<split_cell<T,N,D>> split;

        private :
            explicit any_cell(any_cell<T,N-1,D>& parent) : parent(parent) {}
            any_cell(const any_cell&) = delete;
            any_cell& operator=(const any_cell&) = delete;
            any_cell(any_cell&&) = default;
            any_cell& operator=(any_cell&&) = default;

            /// Called when a child cell is emptied, in order to free
            /// some memory when the space in inoccupied.
            void on_cell_empty_() {
                for (std::size_t i = 0; i < 4; ++i) {
                    if (!split->childs[i].empty())
                        return;
                }

                split.reset();
                parent.on_cell_empty_();
            }

            /// Called by the child cells to build their absolute position.
            /** \param c   The child cell that required the computation
                \param pos Variable holding the position
                \note The position is build recursively. Each cell adds to
                      the position the contribution of its sub-cell.
            */
            void get_coordinates_(const any_cell<T,N+1,D>& c, vec_t& pos) const {
                static const std::size_t half_size = 1 << (D-N-1);

                std::size_t i = &c - &split->childs[0];
                switch (i) {
                case sub_cell::TL :                                         break;
                case sub_cell::TR : pos.x += half_size;                     break;
                case sub_cell::BR : pos.x += half_size; pos.y += half_size; break;
                case sub_cell::BL :                     pos.y += half_size; break;
                }

                parent.get_coordinates_(*this, pos);
            }

            /// Get a sub-cell from a neighboring cell.
            /** \param dir The direction in which the neigborig cell lies
                \param id  The id of the cell to retrieve from this neigbor
                \note Will create the cell if it doesn't exist.
                \note Will return nullptr if the boundaries of the universe have
                      been reached.
            **/
            any_cell<T,N+1,D>* get_neighbor_cell_(direction dir, std::size_t id) {
                any_cell<T,N,D>* next = parent.reach_(*this, dir);
                if (next) {
                    if (!next->split) next->make_split();
                    return &next->split->childs[id];
                } else {
                    return nullptr;
                }
            }

            /// Get a sub-cell from a neighboring cell.
            /** \param dir The direction in which the neigborig cell lies
                \param id  The id of the cell to retrieve from this neigbor
                \note Will not create the cell if it doesn't exist.
                \note Will return nullptr if the boundaries of the universe have
                      been reached.
            **/
            const any_cell<T,N+1,D>* try_get_neighbor_cell_(direction dir, std::size_t id) const {
                const any_cell<T,N,D>* next = parent.try_reach_(*this, dir);
                if (next) {
                    if (!next->split) return nullptr;
                    else return &next->split->childs[id];
                } else {
                    return nullptr;
                }
            }

            /// Get a neigboring sub-cell from one of this cell's sub-cells.
            /** \param c   The cell that requests a neighbor
                \param dir The direction in which to find the neighbor
                \note Will look in neighboring cells if needed.
                \note Will create this neighbor if it doesn't exists.
            **/
            any_cell<T,N+1,D>* reach_(any_cell<T,N+1,D>& c, direction dir) {
                std::size_t next_id;
                if (get_cell_id(&c - &split->childs[0], dir, next_id)) {
                    return &split->childs[next_id];
                } else {
                    return get_neighbor_cell_(dir, next_id);
                }
            }

            /// Get a neigboring sub-cell from one of this cell's sub-cells.
            /** \param c   The cell that requests a neighbor
                \param dir The direction in which to find the neighbor
                \note Will look in neighboring cells if needed.
                \note Will not create this neighbor if it doesn't exists.
            **/
            const any_cell<T,N+1,D>* try_reach_(const any_cell<T,N+1,D>& c, direction dir) const {
                std::size_t next_id;
                if (get_cell_id(&c - &split->childs[0], dir, next_id)) {
                    return &split->childs[next_id];
                } else {
                    return try_get_neighbor_cell_(dir, next_id);
                }
            }
        };

        /// A leaf of the quad-tree, cannot be split, inherits from space::cell.
        template<typename T, std::size_t D>
        struct any_cell<T,D,D> : public space::cell<T> {
            friend split_cell<T,D-1,D>;

            /// @copydoc space::cell::get_coordinates
            space::vec_t get_coordinates() const override {
                static const space::pos_t size = 1 << (D-1);
                vec_t pos(0,0);
                parent.get_coordinates_(*this, pos);
                return space::vec_t(pos) - space::vec_t(size/2, size/2);
            }

            /// @copydoc space::cell::reach
            cell<T>* reach(direction dir) override {
                return parent.reach_(*this, dir);
            }

            /// @copydoc space::cell::try_reach
            const cell<T>* try_reach(direction dir) const override {
                return parent.try_reach_(*this, dir);
            }

            /// The parent of this cell.
            any_cell<T,D-1,D>& parent;

        private :
            explicit any_cell(any_cell<T,D-1,D>& parent) : parent(parent) {}
            any_cell(const any_cell&) = delete;
            any_cell& operator=(const any_cell&) = delete;
            any_cell(any_cell&&) = default;
            any_cell& operator=(any_cell&&) = default;

            /// Called when the content of this cell is destroyed.
            /** Will free some memory if possible, see any_cell::on_cell_empty_.
            **/
            void notify_empty_() override {
                cell<T>::notify_empty_();
                parent.on_cell_empty_();
            }
        };

        /// The root of the quad-tree, can either be empty or split and has no parent.
        template<typename T, std::size_t D>
        struct any_cell<T,1,D> {
            friend any_cell<T,2,D>;
            friend universe<T,D>;

            /// @copydoc space::impl::any_cell::empty
            bool empty() const { return split == nullptr; }

            /// @copydoc space::impl::any_cell::make_split
            void make_split() { split.reset(new split_cell<T,1,D>(*this)); }

            /// The universe this cell belongs to.
            universe<T,D>& parent;

            /// The split sub-cells, if any.
            std::unique_ptr<split_cell<T,1,D>> split;

        private :
            explicit any_cell(universe<T,D>& parent) : parent(parent) {}
            any_cell(const any_cell&) = delete;
            any_cell& operator=(const any_cell&) = delete;
            any_cell(any_cell&&) = default;
            any_cell& operator=(any_cell&&) = default;

            /// @copydoc space::impl::any_cell::on_cell_empty_
            void on_cell_empty_() {
                for (std::size_t i = 0; i < 4; ++i) {
                    if (!split->childs[i].empty())
                        return;
                }

                split.reset();
            }

            /// @copydoc space::impl::any_cell::get_coordinates_
            void get_coordinates_(const any_cell<T,2,D>& c, vec_t& pos) const {
                static const std::size_t half_size = 1 << (D-2);

                std::size_t i = &c - &split->childs[0];
                switch (i) {
                case sub_cell::TL :                                         break;
                case sub_cell::TR : pos.x += half_size;                     break;
                case sub_cell::BR : pos.x += half_size; pos.y += half_size; break;
                case sub_cell::BL :                     pos.y += half_size; break;
                }
            }

            /// @copydoc space::impl::any_cell::reach_
            any_cell<T,2,D>* reach_(any_cell<T,2,D>& c, direction dir) {
                std::size_t next_id;
                if (get_cell_id(&c - &split->childs[0], dir, next_id)) {
                    return &split->childs[next_id];
                } else {
                    return nullptr;
                }
            }

            /// @copydoc space::impl::any_cell::try_reach_
            const any_cell<T,2,D>* try_reach_(const any_cell<T,2,D>& c, direction dir) const {
                std::size_t next_id;
                if (get_cell_id(&c - &split->childs[0], dir, next_id)) {
                    return &split->childs[next_id];
                } else {
                    return nullptr;
                }
            }
        };

        /// Particular case for a universe made out of a single cell...
        /** For completeness, should be useless.
        **/
        template<typename T>
        struct any_cell<T,1,1> : public space::cell<T> {
            friend universe<T,1>;

            /// @copydoc space::impl::any_cell::get_coordinates
            space::vec_t get_coordinates() const override {
                return space::vec_t(0,0);
            }

            /// @copydoc space::impl::any_cell::reach
            cell<T>* reach(direction dir) override {
                return nullptr;
            }

            /// @copydoc space::impl::any_cell::try_reach
            const cell<T>* try_reach(direction dir) const override {
                return nullptr;
            }

            /// @copydoc space::impl::any_cell::parent
            universe<T,1>& parent;

        private :
            explicit any_cell(universe<T,1>& parent) : parent(parent) {}
            any_cell(const any_cell&) = delete;
            any_cell& operator=(const any_cell&) = delete;
            any_cell(any_cell&&) = default;
            any_cell& operator=(any_cell&&) = default;
        };

        /// Universe of depth D.
        template<typename T, std::size_t D>
        struct universe : public space::universe<T> {
            universe() : root_(*this) {}

            /// @copydoc space::universe::depth
            std::size_t depth() const override {
                return D;
            }

            /// @copydoc space::universe::size
            std::size_t size() const override {
                return 1 << (D-1);
            }

            /// @copydoc space::universe::reach
            cell<T>& reach(const space::vec_t& spos) override {
                static const pos_t half_size = 1 << (D-2);
                vec_t pos(spos + space::vec_t(half_size, half_size));
                if (!check_position_(pos)) throw invalid_position_exception(spos);

                return reach_(root_, pos);
            }

            /// @copydoc space::universe::try_reach
            const cell<T>* try_reach(const space::vec_t& spos) const override {
                static const pos_t half_size = 1 << (D-2);
                vec_t pos(spos + space::vec_t(half_size, half_size));
                if (!check_position_(pos)) return nullptr;

                return try_reach_(root_, pos);
            }

            /// @copydoc space::universe::clip
            void clip(const axis_aligned_box2d& box, sorted_vector<const cell<T>&>& list) const override {
                static const pos_t half_size = 1 << (D-2);
                clip_(root_, box + vec2d(half_size, half_size), list);
            }

        private :
            /// The root cell of the quad-tree.
            any_cell<T,1,D> root_;

            /// Return the id of the cell in the Nth level that contains
            /// the provided position, and modify this position for future
            /// calls in order to always clamp it within what is accessible
            /// to this level.
            template<std::size_t N>
            static std::size_t get_id_(vec_t& pos) {
                static const pos_t half_size = 1 << (D-N-1);
                auto dx = pos.x/half_size; pos.x -= dx*half_size;
                auto dy = pos.y/half_size; pos.y -= dy*half_size;
                return dx + 2*dy;
            }

            /// Check that the provided position is valid.
            /** I.e. it doesn't go past the boundaries of this universe.
            **/
            bool check_position_(const vec_t& pos) const {
                static const pos_t size = 1 << (D-1);
                return pos.x < size && pos.y < size;
            }

            /// Recursively traverse the quad-tree to reach the provided position.
            /** \param c   The cell to dive in
                \param pos The position to look for within that cell
                \note Will split a cell if needed in order to reach this position.
            **/
            template<std::size_t N>
            cell<T>& reach_(any_cell<T,N,D>& c, vec_t& pos) {
                if (!c.split) c.make_split();
                return reach_(c.split->childs[get_id_<N>(pos)], pos);
            }

            /// Recursively traverse the quad-tree to reach the provided position.
            /** \param c   The cell to dive in
                \param pos The position to look for within that cell
                \note Will split a cell if needed in order to reach this position.
            **/
            cell<T>& reach_(any_cell<T,D,D>& c, vec_t& pos) {
                return c;
            }

            /// Recursively traverse the quad-tree to reach the provided position.
            /** \param c   The cell to dive in
                \param pos The position to look for within that cell
                \note Will abort if splitting a cell is needed in order to reach this position.
            **/
            template<std::size_t N>
            const cell<T>* try_reach_(const any_cell<T,N,D>& c, vec_t& pos) const {
                if (!c.split) return nullptr;
                return try_reach_(c.split->childs[get_id_<N>(pos)], pos);
            }

            /// Recursively traverse the quad-tree to reach the provided position.
            /** \param c   The cell to dive in
                \param pos The position to look for within that cell
                \note Will abort if splitting a cell is needed in order to reach this position.
            **/
            const cell<T>* try_reach_(const any_cell<T,D,D>& c, vec_t& pos) const {
                return &c;
            }

            /// Recursively traverse the quad-tree to select the cells within the provided box.
            /** \param c    The cell to dive in
                \param box  The box within which to select the cells
                \param list The container within which to store the selected cells
            **/
            template<std::size_t N>
            void clip_(const any_cell<T,N,D>& c, const axis_aligned_box2d& box, sorted_vector<const cell<T>&>& list) const {
                static const double size = 1 << (D-N);
                static const axis_aligned_box2d cell_boxes[4] = {
                    axis_aligned_box2d(vec2d(   0.0,    0.0), vec2d(size/2, size/2)),
                    axis_aligned_box2d(vec2d(size/2,    0.0), vec2d(  size, size/2)),
                    axis_aligned_box2d(vec2d(   0.0, size/2), vec2d(size/2,   size)),
                    axis_aligned_box2d(vec2d(size/2, size/2), vec2d(  size,   size))
                };
                static const vec2d offsets[4] = {
                    vec2d(   0.0,    0.0),
                    vec2d(size/2,    0.0),
                    vec2d(   0.0, size/2),
                    vec2d(size/2, size/2)
                };

                if (!c.split) return;
                for (std::size_t i = 0; i < 4; ++i) {
                    if (box.contains(cell_boxes[i])) {
                        clip_(c.split->childs[i], box - offsets[i], list);
                    }
                }
            }

            /// Recursively traverse the quad-tree to select the cells within the provided box.
            /** \param c    The cell to dive in
                \param box  The box within which to select the cells
                \param list The container within which to store the selected cells
            **/
            void clip_(const any_cell<T,D,D>& c, const axis_aligned_box2d& box, sorted_vector<const cell<T>&>& list) const {
                if (!c.empty()) {
                    list.insert(c);
                }
            }
        };
    }
}

#endif
