#pragma once

#include <atomic>
#include <hetcompute/internal/util/macros.hh>

namespace hetcompute
{
// We need a fast way to disable any memory order optimization to see
// whether a bug could be caused by bad synchronization.
#ifdef HETCOMPUTE_USE_SEQ_CST

    using mem_order                                                       = std::memory_order;
    static HETCOMPUTE_CONSTEXPR_CONST std::memory_order mem_order_relaxed = std::memory_order::memory_order_seq_cst;
    static HETCOMPUTE_CONSTEXPR_CONST std::memory_order mem_order_consume = std::memory_order::memory_order_seq_cst;
    static HETCOMPUTE_CONSTEXPR_CONST std::memory_order mem_order_acquire = std::memory_order::memory_order_seq_cst;
    static HETCOMPUTE_CONSTEXPR_CONST std::memory_order mem_order_release = std::memory_order::memory_order_seq_cst;
    static HETCOMPUTE_CONSTEXPR_CONST std::memory_order mem_order_acq_rel = std::memory_order::memory_order_seq_cst;
    static HETCOMPUTE_CONSTEXPR_CONST std::memory_order mem_order_seq_cst = std::memory_order::memory_order_seq_cst;

#else

    using mem_order                                                       = std::memory_order;
    static HETCOMPUTE_CONSTEXPR_CONST std::memory_order mem_order_relaxed = std::memory_order::memory_order_relaxed;
    static HETCOMPUTE_CONSTEXPR_CONST std::memory_order mem_order_consume = std::memory_order::memory_order_consume;
    static HETCOMPUTE_CONSTEXPR_CONST std::memory_order mem_order_acquire = std::memory_order::memory_order_acquire;
    static HETCOMPUTE_CONSTEXPR_CONST std::memory_order mem_order_release = std::memory_order::memory_order_release;
    static HETCOMPUTE_CONSTEXPR_CONST std::memory_order mem_order_acq_rel = std::memory_order::memory_order_acq_rel;
    static HETCOMPUTE_CONSTEXPR_CONST std::memory_order mem_order_seq_cst = std::memory_order::memory_order_seq_cst;

#endif

}; // namespace hetcompute
