#pragma once

#include <initializer_list>
#include <string>
#include <hetcompute/internal/util/macros.hh>
#include <hetcompute/internal/util/popcount.h>

namespace hetcompute
{
    namespace internal
    {
            ///  This class implements a sparse bitmap using linked lists with
            ///  each node including a size_t bitmap and the position (index) of
            ///  the current element in the link list.  As an example if size_t is
            ///  64bit and if bits 0, 1 and 64 of a are set a = (_bitmap = 0x3,
            ///  _pos = 0) -> (_bitmap = 0x1, __pos = 64) ->NULL
            class sparse_bitmap
            {
            public:
                // Type of hash values
                typedef size_t hash_t;

            private:
                class chunk
                {
                public:
                    // Type of each chunk's bmp
                    typedef size_t bitmap_t;

                    // bits available in each chunk
                    const static size_t s_num_bits = (sizeof(bitmap_t) << 3);

                    bitmap_t _bitmap;
                    size_t   _pos;
                    chunk*   _next;

                    chunk(bitmap_t bmp, size_t pos, chunk* n = nullptr) : _bitmap(bmp), _pos(pos), _next(n)
                    {
                    }

                    chunk() : _bitmap(0), _pos(0), _next(nullptr)
                    {
                    }

                    /// This function is used to change the value of inlined chunk
                    /// @param bmp, initial bitmap
                    /// @param pos, the position
                    /// @param n, next pointer
                    void initialize(bitmap_t bmp, size_t pos, chunk* n = nullptr)
                    {
                        _bitmap = bmp;
                        _pos    = pos;
                        _next   = n;
                    }

                    HETCOMPUTE_DELETE_METHOD(chunk& operator=(chunk& chnk));
                    HETCOMPUTE_DELETE_METHOD(chunk(chunk& chnk));

                    ~chunk()
                    {
                    }
                }; // class chunk

                // The first chunk will be inlined in the sparse_bitmap class and the rest
                // are in a linked list starting from the next pointer of the first chunk
                // The inlined first chunk
                chunk _chunks;

                // The hash value associated with the bitmap
                size_t _hash_value;

                /// Rewrites the bitmap linked list using one single bitmap
                /// This function is used to change the value of inlined chunk
                ///
                /// @param value, initial bitmap
                void set_bitmap(chunk::bitmap_t value)
                {
                    clear();
                    _chunks.initialize(value, 0, nullptr);

                    // This is a special case (_chunks.pos = 0 and _chunks._next = nullptr)
                    // in which the hash value will be equal to the bitmap in the chunk
                    // So I write it this way to save some performance. We can as well just
                    // call compute_hash() instead.
                    _hash_value = static_cast<hash_t>(value);
                }

            public:
                typedef chunk::bitmap_t bitmap_t;

                // Default constructor
                sparse_bitmap() : _chunks(), _hash_value(0)
                {
                }

                // Special mark type used for constructing singleton bitmap
                struct singleton_t
                {
                };
                static singleton_t const singleton;

                /// Constructor
                /// @param value, initial fir chunk (pos 0) bitmap
                explicit sparse_bitmap(size_t value) : _chunks(value, 0, nullptr), _hash_value(static_cast<hash_t>(value))
                {
                }

                /// Constructor for singleton bitmap
                /// @param bit, only set bit index
                sparse_bitmap(size_t bit, singleton_t const&) : _chunks(), _hash_value(0)
                {
                    first_set(bit);
                }

                /// Constructor using initialization set
                /// @param list, set of initial set bits
                explicit sparse_bitmap(std::initializer_list<size_t> list) : _chunks(), _hash_value(0)
                {
                    for (auto& index : list)
                    {
                        set(index);
                    }
                }

                /// Move Constructor
                /// @param other, the bitmap to be moved to this
                sparse_bitmap(sparse_bitmap&& other)
                    : _chunks(other._chunks._bitmap, other._chunks._pos, std::move(other._chunks._next)), _hash_value(other._hash_value)
                {
                    // Making sure other is in a valid state
                    other._chunks.initialize(0, 0, nullptr);
                    other._hash_value = 0;
                }

                /// Copy Constructor
                /// @param other, the bitmap to be copied to this
                sparse_bitmap(const sparse_bitmap& bm) : _chunks(bm._chunks._bitmap, bm._chunks._pos, nullptr), _hash_value(bm._hash_value)
                {
                    // Copy the whole linked list starting from _chunks._next
                    chunk* copy_chunk = bm._chunks._next;
                    chunk* new_chunk  = nullptr;
                    chunk* prev_chunk = nullptr;
                    while (copy_chunk != nullptr)
                    {
                        new_chunk = new chunk(copy_chunk->_bitmap, copy_chunk->_pos, nullptr);
                        if (_chunks._next == nullptr)
                        {
                            _chunks._next = new_chunk;
                        }

                        if (prev_chunk != nullptr)
                        {
                            prev_chunk->_next = new_chunk;
                        }
                        prev_chunk = new_chunk;
                        copy_chunk = copy_chunk->_next;
                    }
                }

                /// Destructor
                ~sparse_bitmap()
                {
                    clear();
                }

                HETCOMPUTE_DELETE_METHOD(sparse_bitmap& operator=(const sparse_bitmap& bm));

                /// Copy operator
                /// @param other, the bitmap to be copied to this
                sparse_bitmap& operator=(sparse_bitmap&& other)
                {
                    clear();
                    // Copy the chunk bitmap and pos,
                    // Perform move semantic on _next linked list
                    _chunks.initialize(other._chunks._bitmap, other._chunks._pos, std::move(other._chunks._next));
                    _hash_value = other._hash_value;
                    // Making sure other is in a valid state
                    other._chunks.initialize(0, 0, nullptr);
                    return *this;
                }

                /// OR Constructor
                /// @param other, the bitmap to be ORed with this
                /// @return the OR result, does not change the operands
                sparse_bitmap operator|(const sparse_bitmap& bm)
                {
                    sparse_bitmap result;
                    result.set_union(*this, bm);
                    return result;
                }

                /// Sets the bit at a given index
                /// @param index, the index of the bit to be set
                /// @return true of the bit is set successfully
                bool set(size_t index);

                /// Similar to set() but for the fastpath for
                /// common case in which the bits in the first chunk are being set
                /// @param index, the index of the bit to be set
                /// @return true of the bit is set successfully
                bool first_set(size_t index)
                {
                    if (index < sparse_bitmap::chunk::s_num_bits)
                    {
                        // The most common case: setting the leave signatures
                        _chunks._bitmap = one_bit_bitmap(index);
                    }
                    else
                    {
                        // Slow path
                        set(index);
                    }
                    return true;
                }

                /// Returns the previously computed hash value of the bitmap
                /// @return the hash value
                hash_t get_hash_value() const
                {
                    return _hash_value;
                }

                /// Clears the linked list and reset of the bits in the bitmap
                void clear()
                {
                    // Walk the bitmap linked list (starting from _chunks._next) and erase
                    chunk* current_chunk = _chunks._next;
                    chunk* next_chunk    = current_chunk;
                    while (current_chunk)
                    {
                        current_chunk = current_chunk->_next;
                        delete next_chunk;
                        next_chunk = current_chunk;
                    }
                    _chunks._bitmap = 0;
                    _chunks._pos    = 0;
                    _chunks._next   = nullptr;
                    _hash_value     = 0;
                }

                /// Returns the number of set bits
                /// @return the bit count
                size_t popcount() const;

                /// Returns true if one bit is set in the bitmap
                /// This is a more efficient implementation than calling popcount
                /// Maybe we can do it more efficiently using ffs or other mechanisms
                /// @return true if this is a singleton bitmap
                bool is_singleton() const
                {
                    // In a singleton we only have the first chunk and there is one bit in it
                    return (_chunks._next == nullptr && (hetcompute_popcountw(_chunks._bitmap) == 1));
                }

                /// Returns true if the bitmap is empty.
                /// The only thing we need to the check is the bitmap of the first chunk
                ///
                /// @return true if the bitmap is empty
                bool is_empty() const
                {
                    return (_chunks._bitmap == 0);
                }

                /// Creates and returns a singleton bitmap
                /// @param index, the bit index to be set in the returned bitmap
                /// @return the singleton bitmap
                inline bitmap_t one_bit_bitmap(size_t index)
                {
                    return (static_cast<bitmap_t>(1) << index);
                }

                /// Inserts a bitmap chunk in the middle of two chunks
                /// @param prev, the chunk to be insert the new one after it
                /// @param next, the chunk to be insert the new one before it
                /// @param bmp, the bitmap of the new chunk
                /// @param new_pos, the position of the new chunk
                chunk* insert_chunk(chunk* prev, chunk* next, bitmap_t bmp, size_t new_pos);

                /// Recomputes the hash value for the entire bitmap linked list
                void recompute_hash();

                /// Equality operator
                /// @param bmp, the bitmap to be compared against to this
                /// @return true if this is equal to bmp
                bool operator==(const sparse_bitmap& bm) const;

                /// Combines two bitmaps and write the value in this object
                /// The content of this bitmap will be modified but not bm1 and bm2
                /// @param bm1, the first bitmap to be unioned
                /// @param bm2, the second bitmap to be unioned
                void set_union(const sparse_bitmap& bm1, const sparse_bitmap& bm2);

                /// Similar to set_union but for fastpath common case in which
                /// the bitmaps have only chunk and chunk positions for both of them is zero
                /// @param bm1, the first bitmap to be unioned
                /// @param bm2, the second bitmap to be unioned
                void fast_set_union(const sparse_bitmap& bm1, const sparse_bitmap& bm2)
                {
                    if (bm1._chunks._pos == 0 && bm1._chunks._next == nullptr && bm2._chunks._pos == 0 && bm2._chunks._next == nullptr)
                    {
                        // Fast path: when both bitmaps are basic bitmap which means they
                        // only occupy the first chunk
                        _chunks._pos    = 0;
                        _chunks._next   = nullptr;
                        _chunks._bitmap = bm1._chunks._bitmap | bm2._chunks._bitmap;
                        _hash_value     = static_cast<hash_t>(_chunks._bitmap);
                    }
                    else
                    {
                        // Slow path
                        set_union(bm1, bm2);
                    }
                }

                /// Returns true if bm is a subset of this object
                /// @param bmp, the bitmap to be compared against to this
                /// @return true if bm is a subset of this
                bool subset(sparse_bitmap const& bm) const;

                /// Creates and returns a text description of this bitmap
                /// @return the bitmap string
                std::string to_string() const;
        };
    };  // namespace internal
};  // namespace hetcompute
