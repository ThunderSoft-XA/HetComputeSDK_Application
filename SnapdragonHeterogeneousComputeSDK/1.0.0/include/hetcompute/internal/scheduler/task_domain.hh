#pragma once

namespace hetcompute
{
    namespace internal
    {
        /// device domain to which task may belong
        enum task_domain
        {
            cpu_all,    // all cpu clusters
            cpu_big,    // big cpu cluster
            cpu_little, // LITTLE cpu cluster
            dsp,        // dsp
            gpu         // gpu
        };
    }; // namespace internal
};     // namespace hetcompute
