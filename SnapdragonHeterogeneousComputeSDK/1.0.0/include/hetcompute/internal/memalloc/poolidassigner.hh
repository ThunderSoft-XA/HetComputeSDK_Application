#pragma once

#include <atomic>
#include <type_traits>
#include <hetcompute/internal/util/memorder.hh>
#include <hetcompute/internal/util/ffs.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace allocatorinternal
        {
            /// This internal class keeps track of pool using a integral type concurrent bitmap.
            template <typename ElementType>
            class pool_id_assigner
            {
                static_assert(std::is_integral<ElementType>::value, "pool_id_assigner type must be integral.");

            public:
                /// Constructor: reset the bitmap
                pool_id_assigner() : _bitmap(0) {}

                /// Destructor
                ~pool_id_assigner() {}

                /// Tries to acquire an id from the available ids
                /// @param id: will contain the allocated id after return
                /// @param pool_count: the total number of allocatable pools
                /// @return true if an unused id is found and allocated
                inline bool acquire(size_t& id, size_t pool_count)
                {
                    while (true)
                    {
                        // Read the bitmap (since later we do fetch_or acq_rel within a loop we use relaxed here)
                        ElementType current_bitmap = _bitmap.load(hetcompute::mem_order_relaxed);

                        // find the next avaialbe bit in the bitmap (zero bit)
                        // ranging from 1 to 32 (0 indicates all bits full)
                        size_t bit_number = hetcompute_ffs(~current_bitmap);

                        // Return if no bit is avaialbe
                        if (bit_number == 0 || bit_number > pool_count)
                        {
                            return false;
                        }

                        size_t bit_index = bit_number - 1;

                        // Try to acquire the bit
                        ElementType mask = 1 << bit_index;
                        current_bitmap   = _bitmap.fetch_or(mask, hetcompute::mem_order_acq_rel);

                        // If the prev bit is 0 and its current value is 1 then done
                        if ((current_bitmap & mask) == 0)
                        {
                            id = bit_index;
                            return true;
                        }
                        // Otherwise, this position was raced and lost to someone else. So try the next available position.
                    }
                }

                /// Releases an id to the available ids
                /// @oparam: the id
                inline void release(size_t id)
                {
                    size_t bit_index = id;
                    _bitmap.fetch_and(~(1 << bit_index), hetcompute::mem_order_acq_rel);
                }

                /// Returns a snapshot of the bitmap
                ElementType get_ids() const { return _bitmap.load(hetcompute::mem_order_relaxed); }

                size_t get_max_ids() { return sizeof(ElementType) * 8; } // 8-bits per byte

            private:
                /// The internal bitmap (bit i is 1 means i-th pool is in use)
                std::atomic<ElementType> _bitmap;
            };

        }; // namespace allocatorinternal

    }; // namespace internal

}; // namespace hetcompute
