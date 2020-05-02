#pragma once

#include <hetcompute/internal/pointkernel/macrodefinitions.hh>
#include <hetcompute/internal/pointkernel/pointkernel-internal.hh>

namespace hetcompute
{
    namespace beta
    {
        // TODO: Add to pointkernels_doc group and add detailed documentation
        /**
         * @brief Create a point-kernel of type <code>T</code>.
         *
         * Creates a pointkernel object of the user specified type.
         *
         * @tparam  T               User type for point kernel
         *
         * @param  global_string <em>Optional</em>, string of global variables,
         *           for the gpu kernel. Default is an empty string.
         *
         * @return A pointkernel object of type T.
         */
        template <typename T>
        typename std::tuple_element<0, T>::type create_point_kernel(std::string global_string = "")
        {
            HETCOMPUTE_UNUSED(global_string);
        }
    }; // namespace beta
}; // namespace hetcompute
    