/** @file hetcompute_error.hh */
#pragma once

namespace hetcompute
{
    /** @addtogroup errorcodes
        @{ */

    /**
       Error codes returned by HetCompute SDK APIs.
       These error codes are applicable only if application
       has disabled exceptions. If application has enabled
       exceptions, hetcompute library will throw exceptions
       instead of returning error codes.
    */
    enum class hc_error : int
    {
        first,

        /**
         * HetCompute API successfully completed
         */
        HC_Success = first,

        /**
         * Error codes pertaining to task execution
         */

        /**
         * Returned by HetCompute runtime if there is a problem with a GPU Kernel.
         */
        HC_TaskGpuFailure,

        /**
         * Returned by HetCompute runtime if there is a problem with a DSP Kernel.
         */
        HC_TaskDspFailure,

        /**
         * Returned by HetCompute runtime if task was canceled.
         */
        HC_TaskCanceled,

        /**
         * Returned by HetCompute runtime if there were multiple failures
         * in a task graph.
         */
        HC_TaskAggregateFailure,

        /**
         * Indicates any error not categorized by the above task failures.
         */
        HC_TaskGenericError,

        /**
         * Error codes pertaining to task execution
         */

        /**
         * HetCompute returns this to indicate group was canceled.
         */
        HC_GroupCanceled,

        /**
         * Returned by HetCompute runtime if there were multiple failures
         * in groups. One possible scenario where this could be returned,
         * is if multiple tasks within a group encountered runtime errors.
         */
        HC_GroupAggregateFailure,

        /**
         * Indicates any error not categorized by the above group failures.
         */
        HC_GroupGenericError,

        last = HC_GroupGenericError
    };

    /** @} */ /* end_addtogroup errorcodes */
}