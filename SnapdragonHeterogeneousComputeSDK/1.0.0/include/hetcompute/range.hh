#pragma once

#include <hetcompute/internal/util/range-utils.hh>
#include <hetcompute/index.hh>

namespace hetcompute
{
    /** @addtogroup rangeclass_doc
    @{ */

    /**
     *  Methods common to 1D, 2D, and 3D ranges.
    */
    template <size_t Dims>
    class range_base
    {
    protected:
        std::array<size_t, Dims> _b;
        std::array<size_t, Dims> _e;
        std::array<size_t, Dims> _s;

    public:
        range_base() : _b(), _e(), _s()
        {
            for (auto& ss : _s)
            {
                ss = 1; // initialize the strides to 1
            }
        }

        range_base(const std::array<size_t, Dims>& bb, const std::array<size_t, Dims>& ee) : _b(bb), _e(ee), _s()
        {
            for (auto& ss : _s)
            {
                ss = 1; // initialize the strides to 1
            }
        }
        range_base(const std::array<size_t, Dims>& bb, const std::array<size_t, Dims>& ee, const std::array<size_t, Dims>& ss)
            : _b(bb), _e(ee), _s(ss)
        {
        }

        /**
         * Returns the beginning of the range in the i-th coordinate.
         * It will cause a fatal error if <code>i</code> is greater than
         * or equal to <code>Dims</code>.
         *
         * @return The beginning of the range in the i-th coordinate. (i < Dims).
         */
        size_t begin(const size_t i) const
        {
            HETCOMPUTE_API_ASSERT(i < Dims, "Index out of bounds");
            return _b[i];
        }

        /**
         * Returns the end of the range in the i-th coordinate.
         * It will cause a fatal error if <code>i</code> is greater than
         * or equal to <code>Dims</code>.
         *
         * @return End of the range in the i-th coordinate. (i < Dims)
         */
        size_t end(const size_t i) const
        {
            HETCOMPUTE_API_ASSERT(i < Dims, "Index out of bounds");
            return _e[i];
        }

        /**
         * Returns the stride the range in the i-th coordinate..
         * It will cause a fatal error if <code>i</code> is greater than
         * or equal to <code>Dims</code>.
         *
         * @return Stride of the range in the i-th coordinate. (i < Dims)
         */
        size_t stride(const size_t i) const
        {
            HETCOMPUTE_API_ASSERT(i < Dims, "Index out of bounds");
            return _s[i];
        }

        /**
         * The number of elements in the i-th coordinate. Equivalent to size(i).
         * @return the number of elements in the i-th coordinate. (i < Dims)
         */
        inline size_t num_elems(const size_t i) const
        {
            HETCOMPUTE_INTERNAL_ASSERT(_e[i] - _b[i], "begin == end");
            return ((_e[i] + (_s[i] - 1) - _b[i]) / _s[i]);
        }

        /**
         * The length of range in the i-th coordinate. num_elems(i) does not
         * take non-stride indices into account, whereas length(i) returns the
         * total length in the i-th coordinate.
         * @return the length of range in the i-th coordinate.
         */
        inline size_t length(const size_t i) const
        {
            HETCOMPUTE_INTERNAL_ASSERT(_e[i] - _b[i], "begin == end");
            return _e[i] - _b[i];
        }

        /**
         * Returns all the coordinates of the beginning of the range.
         * @return An array of all the coordinates at the beginning of the range.
         */
        const std::array<size_t, Dims>& begin() const
        {
            return _b;
        }

        /**
         * Returns all the coordinates of the end of range.
         * @return An array of all the coordinates of the end of the range.
         */
        const std::array<size_t, Dims>& end() const
        {
            return _e;
        }

        /**
         * Returns all the coordinates of the stride of the range.
         * @return An array of all the coordinates of the stride of the range.
         */
        const std::array<size_t, Dims>& stride() const
        {
            return _s;
        }

        /**
         * Returns the dimensions of the range.
         * @return  Dimensions of the range, between [1,3].
         */
        size_t dims() const
        {
            return Dims;
        }
    };

    template <size_t Dims>
    class range;

    /**
      A 1-dimensional range.
      @snippet samples/src/ranges.cc hetcompute::range1d example
    */
    template <>
    class range<1> : public range_base<1>
    {
    public:
        /**
         *  Creates an empty 1D range.
        */
        range() : range_base<1>()
        {
        }

        range(const std::array<size_t, 1>& bb, const std::array<size_t, 1>& ee) : range_base<1>(bb, ee)
        {
        }

        range(const std::array<size_t, 1>& bb, const std::array<size_t, 1>& ee, const std::array<size_t, 1>& ss) : range_base<1>(bb, ee, ss)
        {
        }

        /**
         *  Creates a 1D range, spans from [b0, e0), and is incremented in s0.
         *  It will cause a fatal error if <code>b0</code> is greater than
         *  or equal to <code>e0</code>. <code>s0</code> should be greater than
         *  0.
         *
         *  @param b0 Beginning of 1D range.
         *  @param e0 End of 1D range.
         *  @param s0 Stride of 1D range.
        */
        range(size_t b0, size_t e0, size_t s0)
        {
            HETCOMPUTE_API_ASSERT(((b0 <= e0) && (s0 > 0)), "Invalid range parameters");
            _b[0] = b0;
            _e[0] = e0;
            _s[0] = s0;
        }

        /**
         *  Creates a 1D range, spans from [b0, e0).
         *
         *  It will cause a fatal error if <code>b0</code> is greater
         *  than or equal to <code>e0</code>.
         *
         *  @param b0 Beginning of 1D range.
         *  @param e0 End of 1D range.
        */
        explicit range(size_t b0, size_t e0) : range(b0, e0, 1)
        {
        }

        /**
         *  Creates a 1D range, spans from [0, e0).
         *
         *  @param e0 End of 1D range.
        */
        explicit range(size_t e0) : range(0, e0)
        {
        }

        /**
         * Returns the size of the range.
         * @return Size of the range, product of the number of
         *         elements in each dimension.
        */
        inline size_t size() const
        {
            return num_elems(0);
        }

        /**
         * Returns the linearized distance of the range
         * @return linearized distance of the range, product of length()
         *         in each dimension
         */
        inline size_t linearized_distance() const
        {
            return length(0);
        }

        inline bool is_empty() const
        {
            return (!(_e[0] - _b[0]));
        }

        /**
         * Converts a hetcompute::index<1> object to a linear number with respect to the
         * current range object.
         *
         * @param it hetcompute::index<1> object
         * @return A linear ID with respect to the current range object.
         */
        size_t index_to_linear(const hetcompute::index<1>& it) const
        {
            internal::check_bounds<1>::check(_b, _e, it.data());
            internal::check_stride<1>::check(_b, _s, it.data());
            // Note that after internal::check_stride, it[0] - _b[0] is guranteed to be divisible by _s[0]
            return ((it[0] - _b[0]) / _s[0]);
        }

        /**
         * Converts a linear ID to a hetcompute::index<1>  with respect to the
         * current range object.
         *
         * @param idx Linear ID to be converted to an hetcompute::index<1> object.
         * @return hetcompute::index<1> with respect to the current range object.
        */
        hetcompute::index<1> linear_to_index(size_t idx) const
        {
            index<1> it(idx * _s[0] + _b[0]);
            internal::check_bounds<1>::check(_b, _e, it.data());
            return it;
        }

        void print() const
        {
            HETCOMPUTE_ILOG("(begin:end:stride) [%zu:%zu:%zu]", _b[0], _e[0], _s[0]);
        }
    };

    /**
     *  A 2-dimensional range.
     *
     *  @snippet samples/src/ranges.cc hetcompute::range2d example
     */
    template <>
    class range<2> : public range_base<2>
    {
    public:
        /**
         *  Creates an empty 2D range.
        */
        range() : range_base<2>()
        {
        }

        range(const std::array<size_t, 2>& bb, const std::array<size_t, 2>& ee) : range_base<2>(bb, ee)
        {
            // initialize the stride to 1
            for (auto& ss : _s)
                ss = 1;
        }

        range(const std::array<size_t, 2>& bb, const std::array<size_t, 2>& ee, const std::array<size_t, 2>& ss) : range_base<2>(bb, ee, ss)
        {
        }

        /**
         *  Creates a 2D range, comprising of points from
         *  the cross product [b0:e0:s0) x [0:e1:s1).
         *
         *  It will cause a fatal error if <code>b0</code> is greater
         *  than or equal to <code>e0</code> or if <code>b1</code> is
         *  greater than or equal to <code>e1</code>. <code>s0</code>
         *  and <code>s1</code> should be greater than 0.
         *
         *  @param b0 First coordinate of the beginning of 2D range.
         *  @param e0 First coordinate of the end of 2D range.
         *  @param s0 Stride of the first dimension of 2D range.
         *  @param b1 Second coordinate of the beginning of 2D range.
         *  @param e1 Second coordinate of the end of 2D range.
         *  @param s1 Stride of the second dimension of 2D range.
        */
        range(size_t b0, size_t e0, size_t s0, size_t b1, size_t e1, size_t s1)
        {
            HETCOMPUTE_API_ASSERT(((b0 <= e0) && (b1 <= e1) && (s0 > 0) && (s1 > 0)), "Invalid range parameters");
            _b[0] = b0;
            _b[1] = b1;
            _e[0] = e0;
            _e[1] = e1;
            _s[0] = s0;
            _s[1] = s1;

            HETCOMPUTE_API_ASSERT(((num_elems(0) > 0) && (num_elems(1) > 0)), "Invalid range parameters");
        }

        /**
         *  Creates a 2D range, comprising of points from
         *  the cross product [b0, e0) x [b1, e1).
         *
         *  It will cause a fatal error if <code>b0</code> is greater
         *  than or equal to <code>e0</code> or if <code>b1</code> is
         *  greater than or equal to <code>e1</code>.
         *
         *  @param b0 First coordinate of the beginning of 2D range.
         *  @param e0 First coordinate of the end of 2D range.
         *  @param b1 Second coordinate of the beginning of 2D range.
         *  @param e1 Second coordinate of the end of 2D range.
        */
        range(size_t b0, size_t e0, size_t b1, size_t e1) : range(b0, e0, 1, b1, e1, 1)
        {
        }

        /**
         *  Creates a 2D range, comprising of points from
         *  the cross product [0, e0) x [0, e1).

         *  @param e0 First coordinate of the end of 2D range.
         *  @param e1 Second coordinate of the end of 2D range.
        */
        range(size_t e0, size_t e1) : range(0, e0, 1, 0, e1, 1)
        {
        }

        inline bool is_empty() const
        {
            return (!((_e[0] - _b[0]) && (_e[1] - _b[1])));
        }

        /**
         * Returns the size of the range.
         * @return Size of the range, product of the number of
         *         elements in each dimension.
        */
        inline size_t size() const
        {
            return num_elems(0) * num_elems(1);
        }

        /**
         * Returns the linearized distance of the range
         * @return linearized distance of the range, product of length()
         *         in each dimension
        */
        inline size_t linearized_distance() const
        {
            return length(0) * length(1);
        }

        /**
         * Converts a hetcompute::index<2> object to a linear number with
         * respect to the current range object.
         *
         * @param it hetcompute::index<2> object
         * @return A linear ID with respect to the current range object.
         */
        size_t index_to_linear(const hetcompute::index<2>& it) const
        {
            internal::check_bounds<2>::check(_b, _e, it.data());
            internal::check_stride<2>::check(_b, _s, it.data());
            return ((it[1] - _b[1]) / _s[1]) * num_elems(0) + ((it[0] - _b[0]) / _s[0]);
        }

        /**
         * Converts a linear ID to a hetcompute::index<2>  with respect to the
         * current range object.
         *
         * @param idx Linear ID to be converted to a hetcompute::index<2> object.
         * @return hetcompute::index<2> with respect to the current range object.
         */
        hetcompute::index<2> linear_to_index(size_t idx) const
        {
            size_t   j = idx / num_elems(0);
            size_t   i = idx % num_elems(0);
            index<2> it(i * _s[0] + _b[0], j * _s[1] + _b[1]);

            internal::check_bounds<2>::check(_b, _e, it.data());
            return it;
        }

        void print() const
        {
            HETCOMPUTE_ILOG("(begin:end:stride) [%zu:%zu:%zu], [%zu:%zu:%zu]", _b[0], _e[0], _s[0], _b[1], _e[1], _s[1]);
        }
    };
    /**
     *  A 3-dimensional range.
     *
     *  @snippet samples/src/ranges.cc hetcompute::range3d example
     */
    template <>
    class range<3> : public range_base<3>
    {
    public:
        /**
          Creates an empty 3D range.
        */
        range() : range_base<3>()
        {
        }

        range(const std::array<size_t, 3>& bb, const std::array<size_t, 3>& ee) : range_base<3>(bb, ee)
        {
            for (auto& ss : _s)
                ss = 1;
        }

        range(const std::array<size_t, 3>& bb, const std::array<size_t, 3>& ee, const std::array<size_t, 3>& ss) : range_base<3>(bb, ee, ss)
        {
        }

        /**
         * Creates a 3D range, comprising of points from
         * the cross product [b0:e0:s0) x [b1:e1:s1) x [b2:e2:s2)
         *
         * It will cause a fatal error if <code>b0</code> is greater
         * than or equal to <code>e0</code> or if <code>b1</code> is
         * greater than or equal to <code>e1</code> or if <code>b2</code> is
         * greater than or equal to <code>e2</code>. <code>s0</code>,
         * <code>s1</code> and <code>s2</code> should be greater than 0.
         *
         *  @param b0 First coordinate of the beginning of 3D range.
         *  @param e0 First coordinate of the end of 3D range.
         *  @param s0 Stride of the first dimension of 3D range.
         *  @param b1 Second coordinate of the beginning of 3D range.
         *  @param e1 Second coordinate of the end of 3D range.
         *  @param s1 Stride of the second dimension of 3D range.
         *  @param b2 Third coordinate of the beginning of 3D range.
         *  @param e2 Third coordinate of the end of 3D range.
         *  @param s2 Stride of the third dimension of 3D range.
        */
        range(size_t b0, size_t e0, size_t s0, size_t b1, size_t e1, size_t s1, size_t b2, size_t e2, size_t s2)
        {
            HETCOMPUTE_API_ASSERT(((b0 <= e0) && (b1 <= e1) && (b2 <= e2) && (s0 > 0) && (s1 > 0) && (s2 > 0)), "Invalid range parameters");
            _b[0] = b0;
            _b[1] = b1;
            _b[2] = b2;
            _e[0] = e0;
            _e[1] = e1;
            _e[2] = e2;
            _s[0] = s0;
            _s[1] = s1;
            _s[2] = s2;

            HETCOMPUTE_API_ASSERT(((num_elems(0) > 0) && (num_elems(1) > 0) && (num_elems(2) > 0)), "Invalid range parameters");
        }

        /**
         * Creates a 3D range, comprising of points from
         * the cross product [b0, e0) x [b1, e1) x [b2, e2)
         * It will cause a fatal error if <code>b0</code> is greater
         * than or equal to <code>e0</code> or if <code>b1</code> is
         * greater than or equal to <code>e1</code> or if <code>b2</code> is
         * greater than or equal to <code>e2</code>.
         *
         *  @param b0 First coordinate of the beginning of 3D range.
         *  @param e0 First coordinate of the end of 3D range.
         *  @param b1 Second coordinate of the beginning of 3D range.
         *  @param e1 Second coordinate of the end of 3D range.
         *  @param b2 Third coordinate of the beginning of 3D range.
         *  @param e2 Third coordinate of the end of 3D range.
         */
        range(size_t b0, size_t e0, size_t b1, size_t e1, size_t b2, size_t e2) : range(b0, e0, 1, b1, e1, 1, b2, e2, 1)
        {
        }

        /**
         * Creates a 3D range, comprising of points from
         * the cross product [0, e0) x [0, e1) x [0, e2)
         *
         * @param e0 First coordinate of the end of 3D range.
         * @param e1 Second coordinate of the end of 3D range.
         * @param e2 Third coordinate of the end of 3D range.
        */
        range(size_t e0, size_t e1, size_t e2) : range(0, e0, 1, 0, e1, 1, 0, e2, 1)
        {
        }

        inline bool is_empty() const
        {
            return (!((_e[0] - _b[0]) && (_e[1] - _b[1]) && (_e[2] - _b[2])));
        }

        /**
         * Returns the size of the range.
         * @return Size of the range, product of the number of
         *         elements in each dimension.
        */
        inline size_t size() const
        {
            return num_elems(0) * num_elems(1) * num_elems(2);
        }
        /**
         * Returns the linearized distance of the range
         * @return linearized distance of the range, product of length()
         *         in each dimension
         */

        inline size_t linearized_distance() const
        {
            return length(0) * length(1) * length(2);
        }

        /**
         * Converts a hetcompute::index<3> object to a linear number with the
         * current range object.
         *
         * @param it hetcompute::index<3> object.
         * @return A linear ID with respect to the current range object.
         */
        size_t index_to_linear(const hetcompute::index<3>& it) const
        {
            internal::check_bounds<3>::check(_b, _e, it.data());
            internal::check_stride<3>::check(_b, _s, it.data());
            return (((it[2] - _b[2]) / _s[2]) * num_elems(0) * num_elems(1) + ((it[1] - _b[1]) / _s[1]) * num_elems(0) +
                    ((it[0] - _b[0]) / _s[0]));
        }

        /**
         * Converts a linear ID to a hetcompute::index<3>  with respect to the
         * current range object.
         *
         * @param idx Linear ID to be converted to a hetcompute::index<3> object.
         * @return hetcompute::index<3> with respect to the current range object.
         */
        hetcompute::index<3> linear_to_index(size_t idx) const
        {
            size_t   k = idx / (num_elems(0) * num_elems(1));
            size_t   j = (idx / num_elems(0)) % num_elems(1);
            size_t   i = idx % num_elems(0);
            index<3> it(i * _s[0] + _b[0], j * _s[1] + _b[1], k * _s[2] + _b[2]);

            internal::check_bounds<3>::check(_b, _e, it.data());
            return it;
        }

        void print() const
        {
            HETCOMPUTE_ILOG("(begin:end:stride) [%zu:%zu:%zu], [%zu:%zu:%zu], [%zu:%zu:%zu]",
                            _b[0],
                            _e[0],
                            _s[0],
                            _b[1],
                            _e[1],
                            _s[1],
                            _b[2],
                            _e[2],
                            _s[2]);
        }
    };

    /** @} */ /* end_addtogroup rangeclass_doc */

}; // namespace hetcompute
