#pragma once

#include <array>

namespace hetcompute
{
/** @addtogroup indexclass_doc
@{ */

    /**
      Methods common to 1D, 2D, and 3D index objects are listed here. The
      value for Dims can be 1, 2, or 3.
    */
    template <size_t Dims>
    class index_base
    {
    protected:
        std::array<size_t, Dims> _data;

    public:
        index_base() : _data()
        {
            _data.fill(0);
        }

        virtual ~index_base()
        {
        }

        /**
         * Constructs an index_base object from std::array
         *
         * @param rhs std::array to be used for constructing a new object.
         */
        explicit index_base(const std::array<size_t, Dims>& rhs) : _data(rhs)
        {
        }

        /**
         * Replaces the contents of the current index_base object with an other index_base object.
         *
         * @param rhs index_base object to be used for replacing the contents
         *            of current object.
         */
        index_base<Dims>& operator=(const index_base<Dims>& rhs)
        {
            _data = rhs.data();
            return (*this);
        }

        /**
         * Sums the corresponding values of the current index_base object and another
         * index_base object and returns a reference to the current index_base object.
         *
         * @param rhs index_base object to be used for summing with the values
         *            of current object.
         */
        index_base<Dims>& operator+=(const index_base<Dims>& rhs)
        {
            for (size_t i = 0; i < Dims; i++)
            {
                _data[i] += rhs[i];
            }
            return (*this);
        }

        /**
         * Subtracts the corresponding values of current index_base object and another
         * index_base object and returns a reference to current index_base object.
         *
         * @param rhs index_base object to be used for subtraction with the values
         *            of the current object.
         */
        index_base<Dims>& operator-=(const index_base<Dims>& rhs)
        {
            for (size_t i = 0; i < Dims; i++)
            {
                _data[i] -= rhs[i];
            }
            return (*this);
        }

        /**
         * Subtracts the corresponding values of the current index_base object and another
         * index_base object and returns a new index_base object.
         *
         * @param rhs index_base object to be used for subtraction with the values
         *            of the current object.
         */
        index_base<Dims> operator-(const index_base<Dims>& rhs)
        {
            return (index_base<Dims>(*this) -= rhs);
        }

        /**
         * Sums the corresponding values of the current index_base object and another
         * index_base object and returns a new index_base object.
         *
         * @param rhs index_base object to be used for summing with the values
         *            of the current object.
         */
        index_base<Dims> operator+(const index_base<Dims>& rhs)
        {
            return (index_base<Dims>(*this) += rhs);
        }

        /**
         *   Compares this with another index_base object.
         *
         *   @param rhs Reference to index_base to be compared with this.
         *
         *   @return
         *   TRUE -- The two indices have the same values.\n
         *   FALSE -- The two indices have different values.
         */
        bool operator==(const index_base<Dims>& rhs) const
        {
            return (const_cast<std::array<size_t, Dims>&>(_data) == rhs.data());
        }

        /**
         *   Checks for inequality of this with another index_base object.
         *
         *   @param rhs Reference to index_base to be compared with this.
         *
         *   @return
         *   TRUE -- The two indices have different values.\n
         *   FALSE -- The two indices have the same values.
         *
         */
        bool operator!=(const index_base<Dims>& rhs) const
        {
            return (const_cast<std::array<size_t, Dims>&>(_data) == rhs.data());
        }

        /**
         *   Checks if this object is less than another index_base object.
         *   Performs a lexicographical comparison of two index_base objects,
         *   similar to std::lexicographical_compare().
         *
         *   @param rhs Reference to the index_base to be compared with this.
         *
         *   @return
         *   TRUE -- If this is lexicographically is smaller than rhs.\n
         *   FALSE -- If this is lexicographically is larger or equal to rhs.
         */
        bool operator<(const index_base<Dims>& rhs) const
        {
            return (const_cast<std::array<size_t, Dims>&>(_data) < rhs.data());
        }

        /**
         *   Checks if this object is less than or equal to another index_base object.
         *   Does a lexicographical comparison of two index_base objects,
         *   similar to std::lexicographical_compare().
         *
         *   @param rhs Reference to index_base to be compared with this.
         *
         *   @return
         *   TRUE -- If this is lexicographically is smaller or equal than rhs.\n
         *   FALSE -- If this is lexicographically is larger than rhs.
         */
        bool operator<=(const index_base<Dims>& rhs) const
        {
            return (const_cast<std::array<size_t, Dims>&>(_data) <= rhs.data());
        }

        /**
         *   Checks if this object is greater than another index_base object.
         *   Does a lexicographical comparison of two index_base objects,
         *   similar to std::lexicographical_compare().
         *
         *   @param rhs Reference to index_base to be compared with this.
         *
         *   @return
         *   TRUE -- If this is lexicographically is larger than rhs.\n
         *   FALSE -- If this is lexicographically is smaller or equal to rhs.
         */
        bool operator>(const index_base<Dims>& rhs) const
        {
            return (const_cast<std::array<size_t, Dims>&>(_data) > rhs.data());
        }

        /**
         *   Checks if this object is greater or equal to another index_base object.
         *   Does a lexicographical comparison of two index_base objects,
         *   similar to std::lexicographical_compare().
         *
         *   @param rhs Reference to index_base to be compared with this.
         *
         *   @return
         *   TRUE -- If this is lexicographically is larger or equal to rhs.\n
         *   FALSE -- If this is lexicographically is smaller than rhs.
         */
        bool operator>=(const index_base<Dims>& rhs) const
        {
            return (const_cast<std::array<size_t, Dims>&>(_data) >= rhs.data());
        }

        /**
         * Returns a reference to i-th coordinate of index_base object.
         * No bounds checking is performed.
         *
         * @param i Specifies which coordinate to return.
         * @return Reference to i-th coordinate of the index_base object.
         */
        size_t& operator[](size_t i)
        {
            return _data[i];
        }

        /**
         * Returns a const reference to i-th coordinate of index_base object.
         * No bounds checking is performed.
         *
         * @param i Specifies which coordinate to return.
         * @return Const reference to i-th coordinate of the index_base object.
         */
        const size_t& operator[](size_t i) const
        {
            return _data[i];
        }

        /**
         * Returns a reference to an std::array of all coordinates of index_base object.
         * @return Const reference to an std::array of all
         *               coordinates of an index_base object.
         */
        const std::array<size_t, Dims>& data() const
        {
            return _data;
        }
    };

    template <size_t Dims>
    class index;

    template <>
    class index<1> : public index_base<1>
    {
    public:
        index() : index_base<1>()
        {
        }

        explicit index(const std::array<size_t, 1>& rhs) : index_base<1>(rhs)
        {
        }

        explicit index(size_t i)
        {
            _data[0] = i;
        }

        void print() const
        {
            HETCOMPUTE_ILOG("(%zu)", _data[0]);
        }
    };

    template <>
    class index<2> : public index_base<2>
    {
    public:
        index() : index_base<2>()
        {
        }

        explicit index(const std::array<size_t, 2>& rhs) : index_base<2>(rhs)
        {
        }

        index(size_t i, size_t j)
        {
            _data[0] = i;
            _data[1] = j;
        }

        void print() const
        {
            HETCOMPUTE_ILOG("(%zu, %zu)", _data[0], _data[1]);
        }
    };

    template <>
    class index<3> : public index_base<3>
    {
    public:
        index() : index_base<3>()
        {
        }

        explicit index(const std::array<size_t, 3>& rhs) : index_base<3>(rhs)
        {
        }

        index(size_t i, size_t j, size_t k)
        {
            _data[0] = i;
            _data[1] = j;
            _data[2] = k;
        }

        void print() const
        {
            HETCOMPUTE_ILOG("(%zu, %zu, %zu)", _data[0], _data[1], _data[2]);
        }
    };

    /** @} */ /* end_addtogroup indexclass_doc */
}; // namespace hetcompute
