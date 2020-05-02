#pragma once

#include <array>
#include <stdexcept>
#include <vector>
#include <hetcompute/exceptions.hh>
#include <hetcompute/internal/util/debug.hh>

namespace hetcompute
{
    namespace internal
    {
        template <size_t Dims>
        bool in_bounds(const std::array<size_t, Dims>& b, const std::array<size_t, Dims>& e, const std::array<size_t, Dims>& it)
        {
            for (size_t i = 0; i < Dims; i++)
            {
                if (!((b[i] <= it[i]) && (it[i] < e[i])))
                {
                    return false;
                }
            }
            return true;
        }

        template <size_t Dims>
        bool is_valid_index(const std::array<size_t, Dims>& b, const std::array<size_t, Dims>& s, const std::array<size_t, Dims>& it)
        {
            for (size_t i = 0; i < Dims; i++)
            {
                if ((it[i] - b[i]) % s[i] != 0)
                {
                    return false;
                }
            }
            return true;
        }

        template <size_t Dims>
        struct check_bounds;

        template <>
        struct check_bounds<1>
        {
            static void check(const std::array<size_t, 1>& b, const std::array<size_t, 1>& e, const std::array<size_t, 1>& it)
            {
                bool flag = in_bounds(b, e, it);
                HETCOMPUTE_API_THROW_CUSTOM(flag, std::out_of_range, "it: [%zu], lb: [%zu], ub: [%zu]", it[0], b[0], e[0]);
            }
        };

        template <>
        struct check_bounds<2>
        {
            static void check(const std::array<size_t, 2>& b, const std::array<size_t, 2>& e, const std::array<size_t, 2>& it)
            {
                bool flag = in_bounds(b, e, it);
                HETCOMPUTE_API_THROW_CUSTOM(flag,
                                          std::out_of_range,
                                          "it: [%zu, %zu], lb: [%zu, %zu], ub: [%zu, %zu]",
                                          it[0],
                                          it[1],
                                          b[0],
                                          b[1],
                                          e[0],
                                          e[1]);
            }
        };

        template <>
        struct check_bounds<3>
        {
            static void check(const std::array<size_t, 3>& b, const std::array<size_t, 3>& e, const std::array<size_t, 3>& it)
            {
                bool flag = in_bounds(b, e, it);
                HETCOMPUTE_API_THROW_CUSTOM(flag,
                                          std::out_of_range,
                                          "it: [%zu, %zu, %zu], lb: [%zu, %zu, %zu],"
                                          "ub: [%zu, %zu, %zu]",
                                          it[0],
                                          it[1],
                                          it[2],
                                          b[0],
                                          b[1],
                                          b[2],
                                          e[0],
                                          e[1],
                                          e[2]);
            }
        };

        template <size_t Dims>
        struct check_stride;

        template <>
        struct check_stride<1>
        {
            static void check(const std::array<size_t, 1>& b, const std::array<size_t, 1>& s, const std::array<size_t, 1>& it)
            {
                bool flag = is_valid_index(b, s, it);
                HETCOMPUTE_API_THROW_CUSTOM(flag, std::invalid_argument, "it: [%zu], lb: [%zu], stride: [%zu]", it[0], b[0], s[0]);
            }
        };

        template <>
        struct check_stride<2>
        {
            static void check(const std::array<size_t, 2>& b, const std::array<size_t, 2>& s, const std::array<size_t, 2>& it)
            {
                bool flag = is_valid_index(b, s, it);
                HETCOMPUTE_API_THROW_CUSTOM(flag,
                                          std::invalid_argument,
                                          "it: [%zu, %zu], lb: [%zu, %zu], stride: [%zu, %zu]",
                                          it[0],
                                          it[1],
                                          b[0],
                                          b[1],
                                          s[0],
                                          s[1]);
            }
        };

        template <>
        struct check_stride<3>
        {
            static void check(const std::array<size_t, 3>& b, const std::array<size_t, 3>& s, const std::array<size_t, 3>& it)
            {
                bool flag = is_valid_index(b, s, it);
                HETCOMPUTE_API_THROW_CUSTOM(flag,
                                          std::invalid_argument,
                                          "it: [%zu, %zu, %zu], lb: [%zu, %zu, %zu],"
                                          "stride: [%zu, %zu, %zu]",
                                          it[0],
                                          it[1],
                                          it[2],
                                          b[0],
                                          b[1],
                                          b[2],
                                          s[0],
                                          s[1],
                                          s[2]);
            }
        };
    }; // namespace internal
}; // namespace hetcompute
