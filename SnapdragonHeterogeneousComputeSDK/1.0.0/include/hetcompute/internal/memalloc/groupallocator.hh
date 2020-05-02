#pragma once

#include <atomic>
#include <string>

#include <hetcompute/internal/memalloc/linearpool.hh>
#include <hetcompute/internal/util/ffs.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace allocatorinternal
        {
            /*
             * NOTE 1: A template parameter called MeetSize was not being used, so ignored for now.
             *         But we may need to add it back later to add support for meet groups.
             * NOTE 2: Assuming the maximum number of leaf in the allocator to be 32. May need to increase/parameterize later.
             */
            /// A simple unified allocator for groups. It uses linear allocator for leaves.
            /// Parameters: size of each leaf stored in the allocator.
            template <size_t LeafSize>
            class group_allocator
            {
                /// Size of the leaf pool
                static constexpr size_t s_leaf_pool_size = 32;

            public:
                /// Constructor
                group_allocator() : _leaf_pool() {}

                /// Destructor
                ~group_allocator() {}

                HETCOMPUTE_DELETE_METHOD(group_allocator(group_allocator const&));
                HETCOMPUTE_DELETE_METHOD(group_allocator& operator=(group_allocator const&));
                HETCOMPUTE_DELETE_METHOD(group_allocator(group_allocator const&&));
                HETCOMPUTE_DELETE_METHOD(group_allocator& operator=(group_allocator const&&));

                /// Allocates leaves
                /// @param leaf_id the id of the corresponding leaf
                char* allocate_leaf(size_t leaf_id) { return _leaf_pool.allocate_object(leaf_id); }

                /// Returns a string representation of this allocator
                std::string to_string()
                {
                    std::string str = strprintf("Group allocator leaf pool: %s", _leaf_pool.to_string().c_str());
                    return str;
                }

                /// Returns the size of leaf pool
                static size_t get_leaf_pool_size() { return s_leaf_pool_size; }

            private:
                /// The internal leaf pool
                fixed_size_linear_pool<LeafSize, s_leaf_pool_size> _leaf_pool;
            };

        }; // namespace allocatorinternal

    }; // namespace internal

}; // namespace hetcompute
