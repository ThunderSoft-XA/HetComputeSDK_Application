#pragma once

#include <atomic>
#include <string>

#include <hetcompute/internal/memalloc/alignedmalloc.hh>
#include <hetcompute/internal/util/strprintf.hh>
#include <hetcompute/internal/compat/compat.h>
#include <hetcompute/internal/util/debug.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace allocatorinternal
        {
            /// This class represent a special allocator pool used explicitly by leaf groups.
            /// It is a linear set of objects which are requested using the index (id) of
            /// objects. So the client needs to do most of the book keeping on their side.
            /// Since leaf groups has unique ids created and managed by the group factory,
            /// this class fits for that use case.
            /// Class parameters are the size of each entry in bytes and the number of Entries in
            /// the pool.
            template <size_t EntrySize, size_t numEntries>
            class fixed_size_linear_pool
            {
            public:
                /// Constructor
                fixed_size_linear_pool() : _buffer(nullptr)
                {
                    // this assert is resolved during compile time
                    hetcompute_constexpr_static_assert(EntrySize > 0 && numEntries > 0, "Both numEntries and EntrySize must be larger than zero.");

                    // allocate space for the entries
                    _buffer = reinterpret_cast<char*>(hetcompute_aligned_malloc(POOL_ALIGNMENT, EntrySize * numEntries));
                    if (_buffer == nullptr)
                    {
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                        throw std::bad_alloc();
#else
                        HETCOMPUTE_ELOG("HetCompute Memory Allocation failed in fixed_size_linear_pool");
#endif
                    }
                }

                /// Destructor
                ~fixed_size_linear_pool() { hetcompute_aligned_free(_buffer); }

                /*
                 * Actual allocation is done in constructor, so just return the pointer.
                 */
                /// Returns the object associated with a particular index
                /// @param entry_index: object index
                /// @return the object pointer
                char* allocate_object(size_t entry_index)
                {
                    if (entry_index < numEntries)
                    {
                        return static_cast<char*>(_buffer + entry_index * EntrySize);
                    }
                    return nullptr;
                }

                /// @return the string representation for the pool info
                std::string to_string()
                {
                    std::string str = strprintf("Linear pool: buffer %p, entry size %zu, number of entries %zu", _buffer, EntrySize, numEntries);
                    return str;
                }

                /// @return the start of the internal buffer (for testing)
                char* get_buffer() { return _buffer; };

                HETCOMPUTE_DELETE_METHOD(fixed_size_linear_pool(fixed_size_linear_pool const&));
                HETCOMPUTE_DELETE_METHOD(fixed_size_linear_pool& operator=(fixed_size_linear_pool const&));
                HETCOMPUTE_DELETE_METHOD(fixed_size_linear_pool(fixed_size_linear_pool const&&));
                HETCOMPUTE_DELETE_METHOD(fixed_size_linear_pool& operator=(fixed_size_linear_pool const&&));

            private:
                /// The internal buffer
                char* _buffer;

                /// Default alignment for the malloc
                static constexpr size_t POOL_ALIGNMENT = 128;
            };

        }; // namespace allocatorinternal

    }; // namespace internal

}; // namespace hetcompute
