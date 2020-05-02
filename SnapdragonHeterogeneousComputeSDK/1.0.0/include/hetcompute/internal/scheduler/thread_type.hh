#pragma once

namespace hetcompute
{
    namespace internal
    {
        /// types of threads
        enum thread_type
        {
            foreign, // default type, not created by HETCOMPUTE
            main,    // thread that calls runtime::init
            hetcompute // HETCOMPUTE-created thread
        };
    }; // namespace internal
};     // namespace hetcompute
