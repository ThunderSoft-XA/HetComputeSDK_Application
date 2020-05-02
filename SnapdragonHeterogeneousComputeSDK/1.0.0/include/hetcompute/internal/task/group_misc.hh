#pragma once

#include <atomic>
#include <forward_list>

#include <hetcompute/internal/legacy/types.hh>
#include <hetcompute/internal/task/group_shared_ptr.hh>
#include <hetcompute/internal/task/group_signature.hh>
#include <hetcompute/internal/util/concurrent_dense_bitmap.hh>
#include <hetcompute/internal/util/debug.hh>

namespace hetcompute
{
    namespace internal
    {
        // Forward declarations
        namespace testing
        {
            class group_tester;
        };  // namespace testing

        class group;
        class meet;

        namespace group_misc
        {
            // Forward declarations
            class lattice;
            class lattice_node_leaf;

            // group ID type
            struct id
            {
                typedef size_t type;
            };

            /// group ID generator class
            /// This class keeps track of assigned leaf ids using a dense concurrent
            /// bitmap.
            /// This is used to generate unique ids for leaves concurrently.
            /// Having unique ids for leaves is important as the leaf ids are used to
            /// create bitmaps for meets and so used to guarantee that the meets
            /// are looked up in DB uniquely.
            /// These ids must be assigned to leaves when they are created. The id
            /// associated with a leaf is released when the group is destroyed.
            class id_generator
            {
                static concurrent_dense_bitmap s_bmp;

            public:
                static constexpr id::type default_meet_id = ~id::type(0);

                /// Returns number of set bits in the bitmap (# of leaves)
                static size_t get_num_leaves() { return (s_bmp.popcount()); }

                /// Acquires the next available leave id (empty bit in the bitmap)
                static id::type get_leaf_id() { return (s_bmp.set_first()); }

                /// Releases the next available leave id (empty bit in the bitmap)
                static void release_id(id::type id) { s_bmp.reset(id); }

                friend class testing::group_tester;
            };  // class id_generator

            /// This class represents meet database.
            /// In order to keep track of all the previously created meets in the
            /// lattice, each meet is associated with a leaf owner and that owner leaf
            /// keeps track of its assigned meets in a list (meet database).
            /// During intersection operation, in lookup time (the find function),
            /// the list is traversed linearly until a signature hit is found.
            /// For a meet group, the owner group is the leaf with smallest id
            /// contributing to that meet. Assuming A, B ,C leaves with ids 0, 1, and 2
            /// DB owner of A & B & D = A, DB owner of B & C = B, DB owner of A & C = A
            /// We use a list instead of the map because in common cases, the number
            /// of meets assign to each leaf is small and the overhead of traversing
            /// the list is significantly small as a result of distributing meets
            /// among leaves.
            class meet_db
            {
                /// Friend list (only classes that can construct meet db)
                friend class lattice;
                friend class lattice_node_leaf;

                typedef std::forward_list<group*> meet_list;

            private:
                explicit meet_db(size_t leaf_id) : _leaf_id(leaf_id), _meet_list(){};

            public:
                size_t get_leaf_id() const { return _leaf_id; };
                bool   empty() { return _meet_list.empty(); };

                /// Looks up a meet using its signature
                ///
                /// @return found group (null otherwise)
                group* find(group_signature& sig);

                /// Adds a meet to the DB
                void add(group* g)
                {
#ifdef HETCOMPUTE_DEBUG
                    s_meet_groups_in_db++;
#endif
                    _meet_list.push_front(g);
                }

                /// Removes a meet from the DB (if found)
                void remove(group* g)
                {
#ifdef HETCOMPUTE_DEBUG
                    s_meet_groups_in_db--;
#endif
                    _meet_list.remove(g);
                }

                /// Returns the size of DB
                size_t size() { return std::distance(_meet_list.begin(), _meet_list.end()); }

                HETCOMPUTE_DELETE_METHOD(meet_db(meet_db const&));
                HETCOMPUTE_DELETE_METHOD(meet_db(meet_db&&));
                HETCOMPUTE_DELETE_METHOD(meet_db& operator=(meet_db const&));
                HETCOMPUTE_DELETE_METHOD(meet_db& operator=(meet_db&&));
                static std::atomic<size_t> s_meet_groups_in_db;
                /// All groups added to distributed meet DBs
                static size_t meet_groups_in_db() { return s_meet_groups_in_db.load(); };

            private:
                /// The leaf id of the leaf group holding this list
                /// Storing leaf id here might seem redundant but can
                /// potentially speedup finding the leave with smallest id
                /// An alternative is to use ffs to find the lowest set bit
                /// in a and b bitmaps.
                size_t _leaf_id;
                /// The list of meets tracked in this list
                meet_list _meet_list;
            };  // class meet_db

            /// This class represents a lattice node dynamically constructed
            /// Only constructed for the meets or
            /// the leaves which are the ancestors of some meets
            class lattice_node
            {
                /// Friend list (only classes that can construct lattice_node_leaf)
                friend class ::hetcompute::internal::group;
                friend class ::hetcompute::internal::meet;

            public:
                typedef std::forward_list<group*>           children_list;
                typedef std::forward_list<group_shared_ptr> parent_list;

            protected:
                /// Largest group order
                static const size_t s_max_order = 200;

                /// Group signature
                group_signature _signature;

                /// Group parents and children
                parent_list   _parents;
                children_list _children;

                /// Number of bits in the bitmap.
                size_t _order;

                /// Meet database is the list owned by the
                /// contributing leaf with smallest id.
                group_misc::meet_db* _meet_db;

                /// Set if the tasks counter is propagated to the parents
                bool _parent_sees_tasks;

            protected:
                /// Constructor (can only be used by group or meet)
                /// only for leaf lattice nodes
                ///
                /// @param id: leaf id
                /// @param db: pointer to the leaf inlined meet_db
                lattice_node(size_t id, size_t order, group_misc::meet_db* db)
                    : _signature(id, sparse_bitmap::singleton), _parents(), _children(), _order(order), _meet_db(db), _parent_sees_tasks(false)
                {
                    HETCOMPUTE_API_ASSERT(_order <= s_max_order, "Too many groups intersected. Max is %zu.", s_max_order);
                }

                /// Constructor (can only be used by group or meet)
                /// only for meet db of leaves
                ///
                /// @param id: leaf id
                /// @param order: the order of the node in the lattice
                /// @param db: pointer to the leaf inlined meet_db
                lattice_node(group_signature& signature, size_t order, group_misc::meet_db* db)
                    : _signature(std::move(signature)), _parents(), _children(), _order(order), _meet_db(db), _parent_sees_tasks(false)
                {
                    HETCOMPUTE_API_ASSERT(_order <= s_max_order, "Too many groups intersected. Max is %zu.", s_max_order);
                }

                /// Constructor (can only be used by group or meet)
                /// only for meet db of leaves
                ///
                /// @param id: leaf id
                /// @param order: the order of the node in the lattice
                /// @param db: pointer to the leaf inlined meet_db
                lattice_node(size_t order, group_misc::meet_db* db)
                    : _signature(), _parents(), _children(), _order(order), _meet_db(db), _parent_sees_tasks(false)
                {
                    HETCOMPUTE_API_ASSERT(_order <= s_max_order, "Too many groups intersected. Max is %zu.", s_max_order);
                }

                HETCOMPUTE_DELETE_METHOD(lattice_node(lattice_node const&));
                HETCOMPUTE_DELETE_METHOD(lattice_node(lattice_node&&));
                HETCOMPUTE_DELETE_METHOD(lattice_node& operator=(lattice_node const&));
                HETCOMPUTE_DELETE_METHOD(lattice_node& operator=(lattice_node&&));

            public:
                virtual ~lattice_node();

                /// Returns the parent_sses_task flag
                bool get_parent_sees_tasks() { return _parent_sees_tasks; };

                /// Returns the parent list
                parent_list& get_parents() { return _parents; };

                /// Returns the children list
                children_list& get_children() { return _children; };

                /// Sets the parent_sses_task flag
                void set_parent_sees_tasks(bool value) { _parent_sees_tasks = value; };

                /// Returns meet_db
                group_misc::meet_db* get_meet_db() { return _meet_db; };

                /// Returns the order in lattice
                size_t get_order() { return _order; };

                /// Returns the group signature
                group_signature& get_signature() { return _signature; };

                /// Creates the meet list on demand when there is a new meet
                /// that will end up in this list
                void set_meet_db(group_misc::meet_db* db)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_meet_db == nullptr, "Meet database must be null inorder to be created");
                    _meet_db = db;
                };

                /// Checks whether the group is a leaf in a group tree
                bool is_for_leaf() const { return (_order == 1); }

            }; // lattice_node

            class lattice_node_leaf : public lattice_node
            {
            protected:
                /// Friend list (only classes that can construct lattice_node_leaf)
                friend class ::hetcompute::internal::group;
                /// Constructor (can only be used by group)
                /// only for meet db of leaves
                ///
                /// @param id: leaf id
                explicit lattice_node_leaf(size_t id) : lattice_node(id, 1, &_inlined_meet_db), _inlined_meet_db(id) {}

            private:
                group_misc::meet_db _inlined_meet_db;
            };

        };  // namespace group_misc
    };  // namespace internal
};  // namespace hetcompute
