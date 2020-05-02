#pragma once

#include <hetcompute/internal/buffer/buffer-internal.hh>
#include <hetcompute/internal/util/debug.hh>

namespace hetcompute
{
    template <typename T>
    class buffer_const_iterator;

    /** @addtogroup buffer_doc
        @{ */

    /**
     * @brief Random access iterator over a buffer.
     *
     * Random access iterator over a buffer.
     *
     * @sa <code>hetcompute::buffer_ptr::iterator</code>
     * @sa <code>hetcompute::buffer_ptr::begin()</code>
     * @sa <code>hetcompute::buffer_ptr::end()</code>
     */
    template <typename T>
    class buffer_iterator
    {
        /** @cond HIDE_FROM_DOXYGEN */
        buffer_ptr<T>* _p_buffer_ptr;
        size_t         _index;

        buffer_iterator(buffer_ptr<T>* p_buffer_ptr, size_t index) : _p_buffer_ptr(p_buffer_ptr), _index(index)
        {
        }

        friend class buffer_ptr<T>;
        friend class buffer_const_iterator<T>;
        /** @endcond */ /* HIDE_FROM_DOXYGEN */

    public:
        T& operator*() const
        {
            return (*_p_buffer_ptr)[_index];
        }

        T& operator[](size_t n) const
        {
            return (*_p_buffer_ptr)[_index + n];
        }

        // prefix
        buffer_iterator& operator++()
        {
            ++_index;
            return *this;
        }

        // prefix
        buffer_iterator& operator--()
        {
            --_index;
            return *this;
        }

        // postfix
        buffer_iterator operator++(int)
        {
            auto current = *this;
            ++(*this);
            return current;
        }

        // postfix
        buffer_iterator operator--(int)
        {
            auto current = *this;
            --(*this);
            return current;
        }

        buffer_iterator& operator+=(size_t offset)
        {
            _index += offset;
            return *this;
        }

        buffer_iterator& operator-=(size_t offset)
        {
            _index -= offset;
            return *this;
        }

        buffer_iterator operator+(size_t offset)
        {
            auto updated = *this;
            updated += offset;
            return updated;
        }

        buffer_iterator operator-(size_t offset)
        {
            auto updated = *this;
            updated -= offset;
            return updated;
        }

        int operator-(buffer_iterator const& it)
        {
            return static_cast<int>(_index) - static_cast<int>(it._index);
        }

        bool operator<(buffer_iterator const& it) const
        {
            HETCOMPUTE_INTERNAL_ASSERT(it._p_buffer_ptr != nullptr, "buffer_iterator set to a null buffer");
            HETCOMPUTE_INTERNAL_ASSERT(_p_buffer_ptr == it._p_buffer_ptr, "buffer_iterator mismatch: %p %p", _p_buffer_ptr, it._p_buffer_ptr);
            return _index < it._index;
        }

        bool operator>(buffer_iterator const& it) const
        {
            HETCOMPUTE_INTERNAL_ASSERT(_p_buffer_ptr == it._p_buffer_ptr, "buffer_iterator mismatch: %p %p", _p_buffer_ptr, it._p_buffer_ptr);
            return _index > it._index;
        }

        bool operator<=(buffer_iterator const& it) const
        {
            HETCOMPUTE_INTERNAL_ASSERT(_p_buffer_ptr == it._p_buffer_ptr, "buffer_iterator mismatch: %p %p", _p_buffer_ptr, it._p_buffer_ptr);
            return _index <= it._index;
        }

        bool operator>=(buffer_iterator const& it) const
        {
            HETCOMPUTE_INTERNAL_ASSERT(_p_buffer_ptr == it._p_buffer_ptr, "buffer_iterator mismatch: %p %p", _p_buffer_ptr, it._p_buffer_ptr);
            return _index >= it._index;
        }

        bool operator==(buffer_iterator const& it) const
        {
            HETCOMPUTE_INTERNAL_ASSERT(_p_buffer_ptr == it._p_buffer_ptr, "buffer_iterator mismatch: %p %p", _p_buffer_ptr, it._p_buffer_ptr);
            return _index == it._index;
        }

        bool operator!=(buffer_iterator const& it) const
        {
            HETCOMPUTE_INTERNAL_ASSERT(_p_buffer_ptr == it._p_buffer_ptr, "buffer_iterator mismatch: %p %p", _p_buffer_ptr, it._p_buffer_ptr);
            return _index != it._index;
        }

        HETCOMPUTE_DEFAULT_METHOD(buffer_iterator(buffer_iterator const&));
        HETCOMPUTE_DEFAULT_METHOD(buffer_iterator& operator=(buffer_iterator const&));
        HETCOMPUTE_DEFAULT_METHOD(buffer_iterator(buffer_iterator&&));
        HETCOMPUTE_DEFAULT_METHOD(buffer_iterator& operator=(buffer_iterator&&));
    };

    /**
     * @brief Const random access iterator over a buffer.
     *
     * Const random access iterator over a buffer.
     *
     * @sa <code>hetcompute::buffer_ptr::const_iterator</code>
     * @sa <code>hetcompute::buffer_ptr::cbegin()</code>
     * @sa <code>hetcompute::buffer_ptr::cend()</code>
     */
    template <typename T>
    class buffer_const_iterator
    {
        /** @cond HIDE_FROM_DOXYGEN */
        buffer_ptr<T> const* _p_buffer_ptr;
        size_t               _index;

        buffer_const_iterator(buffer_ptr<T> const* p_buffer_ptr, size_t index) : _p_buffer_ptr(p_buffer_ptr), _index(index)
        {
        }

        friend class buffer_ptr<T>;

        /** @endcond */ /* HIDE_FROM_DOXYGEN */

    public:
        /* implicit */ buffer_const_iterator(buffer_iterator<T> const& it) : _p_buffer_ptr(it._p_buffer_ptr), _index(it._index)
        {
        }

        T const& operator*() const
        {
            return (*_p_buffer_ptr)[_index];
        }

        T const& operator[](size_t n) const
        {
            return (*_p_buffer_ptr)[_index + n];
        }

        // prefix
        buffer_const_iterator& operator++()
        {
            ++_index;
            return *this;
        }

        // prefix
        buffer_const_iterator& operator--()
        {
            --_index;
            return *this;
        }

        // postfix
        buffer_const_iterator operator++(int)
        {
            auto current = *this;
            ++(*this);
            return current;
        }

        // postfix
        buffer_const_iterator operator--(int)
        {
            auto current = *this;
            --(*this);
            return current;
        }

        buffer_const_iterator& operator+=(size_t offset)
        {
            _index += offset;
            return *this;
        }

        buffer_const_iterator& operator-=(size_t offset)
        {
            _index -= offset;
            return *this;
        }

        buffer_const_iterator operator+(size_t offset)
        {
            auto updated = *this;
            updated += offset;
            return updated;
        }

        buffer_const_iterator operator-(size_t offset)
        {
            auto updated = *this;
            updated -= offset;
            return updated;
        }

        int operator-(buffer_const_iterator const& it)
        {
            return static_cast<int>(_index) - static_cast<int>(it._index);
        }

        bool operator<(buffer_const_iterator const& it) const
        {
            HETCOMPUTE_INTERNAL_ASSERT(it._p_buffer_ptr != nullptr, "buffer_const_iterator set to a null buffer");
            HETCOMPUTE_INTERNAL_ASSERT(_p_buffer_ptr == it._p_buffer_ptr,
                                     "buffer_const_iterator mismatch: %p %p",
                                     _p_buffer_ptr,
                                     it._p_buffer_ptr);
            return _index < it._index;
        }

        bool operator>(buffer_const_iterator const& it) const
        {
            HETCOMPUTE_INTERNAL_ASSERT(_p_buffer_ptr == it._p_buffer_ptr,
                                     "buffer_const_iterator mismatch: %p %p",
                                     _p_buffer_ptr,
                                     it._p_buffer_ptr);
            return _index > it._index;
        }

        bool operator<=(buffer_const_iterator const& it) const
        {
            HETCOMPUTE_INTERNAL_ASSERT(_p_buffer_ptr == it._p_buffer_ptr,
                                     "buffer_const_iterator mismatch: %p %p",
                                     _p_buffer_ptr,
                                     it._p_buffer_ptr);
            return _index <= it._index;
        }

        bool operator>=(buffer_const_iterator const& it) const
        {
            HETCOMPUTE_INTERNAL_ASSERT(_p_buffer_ptr == it._p_buffer_ptr,
                                     "buffer_const_iterator mismatch: %p %p",
                                     _p_buffer_ptr,
                                     it._p_buffer_ptr);
            return _index >= it._index;
        }

        bool operator==(buffer_const_iterator const& it) const
        {
            HETCOMPUTE_INTERNAL_ASSERT(_p_buffer_ptr == it._p_buffer_ptr,
                                     "buffer_const_iterator mismatch: %p %p",
                                     _p_buffer_ptr,
                                     it._p_buffer_ptr);
            return _index == it._index;
        }

        bool operator!=(buffer_const_iterator const& it) const
        {
            HETCOMPUTE_INTERNAL_ASSERT(_p_buffer_ptr == it._p_buffer_ptr,
                                     "buffer_const_iterator mismatch: %p %p",
                                     _p_buffer_ptr,
                                     it._p_buffer_ptr);
            return _index != it._index;
        }

        HETCOMPUTE_DEFAULT_METHOD(buffer_const_iterator(buffer_const_iterator const&));
        HETCOMPUTE_DEFAULT_METHOD(buffer_const_iterator& operator=(buffer_const_iterator const&));
        HETCOMPUTE_DEFAULT_METHOD(buffer_const_iterator(buffer_const_iterator&&));
        HETCOMPUTE_DEFAULT_METHOD(buffer_const_iterator& operator=(buffer_const_iterator&&));
    };

    /** @} */ /* end_addtogroup buffer_doc */

}; // namespace hetcompute
