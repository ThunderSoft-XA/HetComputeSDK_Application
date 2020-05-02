
#pragma once

HETCOMPUTE_GCC_IGNORE_BEGIN("-Weffc++");

namespace hetcompute
{
    namespace beta
    {
        /** @addtogroup kernelclass_doc
            @{ */
        /**
         *  @brief Utility base template to get the tuple type.
         *
         *  Utility template to get the tuple type of
         *  hetcompute::range, and other types.
         *
         *  The wrapped type can be accessed trough call_tuple<...>::type.
         *
         *  @tparam Dim dimension of hetcompute::range.
         *  @tparam Args... other types.
         */
        template <size_t Dim, typename... Args>
        struct call_tuple;

        /** @} */ /* end_addtogroup kernelclass_doc */

    };  // namespace beta
};  // namespace hetcompute

HETCOMPUTE_GCC_IGNORE_END("-Weffc++");
