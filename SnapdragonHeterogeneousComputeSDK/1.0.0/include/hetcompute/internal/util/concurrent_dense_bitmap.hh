#pragma once

#include <array>
#include <atomic>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/memorder.hh>
#include <hetcompute/internal/util/popcount.h>

namespace hetcompute
{
    namespace internal
    {
        namespace testing
        {
            class concurrent_dense_bitmap_tester;
        }; // namespace testing

        /**
          This class implements a concurrent dense bitmap using linked lists
          with each node including a size_t array bitmaps and the next element
          in the link list.
         */
        class concurrent_dense_bitmap
        {
            /**
              This struct implements elements in a concurrent dense bitmap
             */
            struct chunk
            {
                // We use a uint32_t element because popcount and ffs are
                // specialize for 32-bit ints.  If 64-bit needed we may switch to
                // hetcompute_popcountw

                typedef uint32_t                  element_type;
                typedef std::atomic<element_type> atomic_element_type;

                constexpr static size_t s_num_elements     = 4;
                constexpr static size_t s_bits_per_element = (sizeof(element_type) << 3);
                constexpr static size_t s_num_bits         = s_bits_per_element * s_num_elements;

                std::atomic<chunk*> _next;
                std::array<atomic_element_type, s_num_elements> _bitmaps;

                /** @brief Constructor. Initializes all entries to 0 */
                chunk() : _next(nullptr), _bitmaps()
                {
                    for (auto& elem : _bitmaps)
                    {
                        elem = 0;
                    }
                }

                ~chunk()
                {
                }

                /** @brief Finds and sets first zero bit. */
                bool set_first(size_t& pos);

                /** @brief Resets bit at pos */
                inline void reset(size_t pos);

                /** Returns bit at pos */
                inline bool test(size_t pos) const;

                /** Returns number of set bits */
                size_t popcount() const;

                HETCOMPUTE_DELETE_METHOD(chunk& operator=(chunk& chnk));
                HETCOMPUTE_DELETE_METHOD(chunk(chunk const&));
            };

            chunk _chunk;

            inline chunk* find_chunk(size_t num_chunk);
            inline const chunk* find_chunk_ro(size_t num_chunk) const;

            HETCOMPUTE_DELETE_METHOD(concurrent_dense_bitmap& operator=(concurrent_dense_bitmap& other));
            HETCOMPUTE_DELETE_METHOD(concurrent_dense_bitmap(concurrent_dense_bitmap const& other));

        public:
            typedef testing::concurrent_dense_bitmap_tester tester;

            /** @brief Constructor*/
            concurrent_dense_bitmap() : _chunk()
            {
            }

            /** Destructor */
            ~concurrent_dense_bitmap();

            /** @brief Sets first zero bit and returns the corresponding index */
            size_t set_first();

            /** @brief Resets bit at index */
            void reset(size_t index);

            /** Returns bit at index */
            bool test(size_t index) const;

            /** Returns number of set bits */
            size_t popcount();

            friend class testing::concurrent_dense_bitmap_tester;

        }; // class concurrent_dense_bitmap

        ///////////////////////////////////////////////////////////////////////////////

        inline concurrent_dense_bitmap::chunk* concurrent_dense_bitmap::find_chunk(size_t chunk_index)
        {
            chunk* curr_chunk = &_chunk;

            // The map only grows. This would have to change if we shrink
            for (size_t i = 0; i < chunk_index; ++i)
            {
                curr_chunk = curr_chunk->_next.load(hetcompute::mem_order_acquire);
            }

            return curr_chunk;
        }

        ///////////////////////////////////////////////////////////////////////////////

        inline const concurrent_dense_bitmap::chunk* concurrent_dense_bitmap::find_chunk_ro(size_t chunk_index) const
        {
            const chunk* curr_chunk = &_chunk;

            // The map only grows. This would have to change if we shrink
            for (size_t i = 0; i < chunk_index; ++i)
            {
                curr_chunk = curr_chunk->_next.load(hetcompute::mem_order_acquire);
            }

            return curr_chunk;
        }

        ///////////////////////////////////////////////////////////////////////////////

        inline void concurrent_dense_bitmap::chunk::reset(size_t pos)
        {
            size_t num_elem  = pos / s_bits_per_element;
            size_t bit_index = pos % s_bits_per_element;
            _bitmaps[num_elem].fetch_and(~(1 << bit_index), hetcompute::mem_order_acq_rel);
        }

        ///////////////////////////////////////////////////////////////////////////////

        inline bool concurrent_dense_bitmap::chunk::test(size_t pos) const
        {
            size_t num_element = pos / s_bits_per_element;
            size_t bit_index   = pos % s_bits_per_element;

            size_t element = _bitmaps[num_element].load(hetcompute::mem_order_acquire);
            size_t mask    = 1 << bit_index;

            return static_cast<size_t>((element & mask) > 0);
        }

        ///////////////////////////////////////////////////////////////////////////////

    }; // namespace internal
}; // namespace hetcompute
