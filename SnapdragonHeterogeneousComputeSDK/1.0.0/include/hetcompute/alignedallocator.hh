/** @file alignedallocator.hh */
#pragma once

// Based on:
// https://code.google.com/p/mastermind-strategy/
//   source/browse/trunk/lib/util/aligned_allocator.hpp
//
// Copyright (c) <year> <copyright holders>
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#include <memory>

#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/memalloc/alignedmalloc.hh>

namespace hetcompute
{
    /** @cond PRIVATE */
    /**
     *  STL-compliant allocator that allocates aligned memory.
     *  @tparam T Type of the element to allocate.
     *  @tparam Alignment Alignment of the allocation, e.g. 16.
     *  @ingroup AlignedAllocator
     */
    template <class T, size_t Alignment>
    // Inherit construct(), destruct() etc.
    struct aligned_allocator : public std::allocator<T>
    {
        typedef typename std::allocator<T>::size_type     size_type;
        typedef typename std::allocator<T>::pointer       pointer;
        typedef typename std::allocator<T>::const_pointer const_pointer;

        /**
         * Defines an aligned allocator suitable for allocating elements of type U.
         */
        template <class U>
        struct rebind
        {
            typedef aligned_allocator<U, Alignment> other;
        };

        /**
         * Default-constructs an allocator.
         */
        aligned_allocator() {}

        /**
         * Copy-constructs an allocator.
         */
        aligned_allocator(const aligned_allocator& other) : std::allocator<T>(other) {}

        /**
         * Convert-constructs an allocator.
         */
        template <class U>
        aligned_allocator(const aligned_allocator<U, Alignment>&)
        {
        }

        /**
         * Destroys an allocator.
         */
        ~aligned_allocator() {}

        /**
         * Allocates n elements of type T, aligned to a multiple of Alignment.
         */
        pointer allocate(size_type n) { return allocate(n, const_pointer(0)); }

        /**
         * Allocates n elements of type T, aligned to a multiple of Alignment.
         */
        pointer allocate(size_type n, const_pointer)
        {
            void* p;
            p = internal::hetcompute_aligned_malloc(Alignment, n * sizeof(T));
            if (!p)
            {
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                throw std::bad_alloc();
#else
                HETCOMPUTE_ELOG("hetcompute_aligned_malloc failed");
#endif
            }

            return static_cast<pointer>(p);
        }

        /**
         * Frees the memory previously allocated by an aligned allocator.
         */
        void deallocate(pointer p, size_type) { internal::hetcompute_aligned_free(p); }
    };

    /**
     *  Checks whether two aligned allocators are equal. Two allocators are equal
     *  if the memory allocated using one allocator can be deallocated by the other.
     *  @returns Always true.
     *  @ingroup AlignedAllocator
     */
    template <class T1, size_t A1, class T2, size_t A2>
    bool operator==(const aligned_allocator<T1, A1>&, const aligned_allocator<T2, A2>&)
    {
        return true;
    }

    /**
     *  Checks whether two aligned allocators are not equal. Two allocators are
     *  equal if the memory allocated using one allocator can be deallocated by
     *  the other.
     *  @returns Always false.
     *  @ingroup AlignedAllocator
     */
    template <class T1, size_t A1, class T2, size_t A2>
    bool operator!=(const aligned_allocator<T1, A1>&, const aligned_allocator<T2, A2>&)
    {
        return false;
    }

    /** @endcond */
}; // namespace hetcompute
