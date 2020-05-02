#pragma once

#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/tls.hh>

namespace hetcompute
{
    namespace internal
    {
        enum class hetcompute_runtime_state_type
        {
            stopped, 
            initializing, 
            initialized
        };
        extern hetcompute_runtime_state_type hetcompute_runtime_state;

        class scheduler;            // forward declaration for send_to_runtime
        class task_bundle_dispatch; // forward declaration for send_to_runtime
        /**
        * Sends task to the appropriate runtime. You can use the 'sched'
        * parameter to indicate which scheduler you would like to send the
        * task to. This is not a guarantee that the chosen scheduler will
        * indeed execute the task, as other factors (i.e. task stealing)
        * might get into play. If you can, please specify a scheduler.
        * Your performance numbers will appreciate it.
        *
        * @param task Pointer to task. Can't be null.
        * @param sched Scheduler where you would like to execute the task.
        * @param notify Set to true to notify device threads, else false
        * @param tbd Schedule task into this task bundle
        */
        void send_to_runtime(task* task, scheduler* sched = nullptr, bool notify = true, task_bundle_dispatch* tbd = nullptr);
        
        /**
         * Sends 'num_instances' number of task to the appropriate runtime.
         * Optimization for pushing indirect tasks for tight loop execution.
         *
         * @param task Pointer to task, must be an indirect task.
         * @param num_instances Number of indirect tasks to push to runtime.
         */
        void send_to_runtime(task* task, size_t num_instances);

        /**
         * Notify the runtime system that tasks have been sent to the runtime
         * @param nelem number of tasks added since last notification
         */
        void notify_all(size_t nelem);

        /**
         * Returns a "loosely consistent" value for the current occupancy of the task
         * queue associated with the thread making the call. May return non-zero only
         * for device threads.
         * @returns Occupancy of task queue associated with thread making the call;
         *          returned value may be stale
         */
        size_t get_load_on_current_thread();

        // The number of execution contexts used by Hetcompute
        extern size_t g_cpu_num_exec_ctx;
        // The number of big execution contexts used by Hetcompute in a big.LITTLE SoC
        extern size_t g_cpu_num_big_exec_ctx;

        // for tuner.hh
        inline size_t num_execution_contexts() { return g_cpu_num_exec_ctx; }

        /**
         * Get number of big execution contexts in Hetcompute in a big.LITTLE SoC
         * Must be called after runtime::init()
         */
        inline size_t num_big_execution_contexts() { return g_cpu_num_big_exec_ctx; }

        /**
         * Get number of LITTLE execution contexts in HetCompute in a big.LITTLE SoC
         * Must be called after runtime::init()
        */
        inline size_t num_little_execution_contexts()
        {
            HETCOMPUTE_INTERNAL_ASSERT(g_cpu_num_exec_ctx >= g_cpu_num_big_exec_ctx,
                "there must be at least as many cpu execution contexts as big cpu execution contexts");
            return g_cpu_num_exec_ctx - g_cpu_num_big_exec_ctx;
        }
        
#if defined(HETCOMPUTE_HAVE_QTI_DSP)
        // The number of dsp threads in the system
        extern size_t g_dsp_num_exec_ctx;
        
        /**
         * Get number of dsp execution contexts in Hetcompute.
         * Must be called after runtime::init()
         */
        inline size_t num_dsp_execution_contexts() { return g_dsp_num_exec_ctx; }
#endif // defined(HETCOMPUTE_HAVE_QTI_DSP)

        // Do not use mutex here for performance reasons, the only time the mutex
        // is needed is for making sure init and shutdown are done atomically.
        inline bool runtime_available()
        {
            return internal::hetcompute_runtime_state == internal::hetcompute_runtime_state_type::initialized;
        }

        inline bool runtime_stopped()
        {
            return internal::hetcompute_runtime_state == internal::hetcompute_runtime_state_type::stopped;
        }

        inline bool runtime_initializing()
        {
            return internal::hetcompute_runtime_state == internal::hetcompute_runtime_state_type::initializing;
        }

        inline bool runtime_not_stopped()
        {
            return runtime_initializing() || runtime_available();
        }

#ifndef _MSC_VER
        // Handler called during fork() that we use to prevent this call, since fork()
        // does not copy the thread pool over, it is important that we never do this
        // while HetCompute is running!
        void prefork_handler();
#endif // _MSC_VER

        /*
         * Internal API to disable/enable Analytics. This is used by performance tests,
         * not exposed with external headers
         */
        void supportAnalytics(bool enable);
        
        void init_impl(bool enableExceptions);

    };  // namespace internal
};  // namespace hetcompute
