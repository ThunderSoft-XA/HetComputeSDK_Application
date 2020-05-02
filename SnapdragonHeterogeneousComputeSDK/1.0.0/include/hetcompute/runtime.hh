/** @file runtime.hh */
#pragma once

#include <hetcompute/internal/runtime-internal.hh>

namespace HetComputeApp
{
    class features
    {
    public:
        static bool supportException()
        {
#ifdef HETCOMPUTE_DISABLE_EXCEPTIONS
            return false;
#else
            return true;
#endif
        }
    };
}

namespace hetcompute
{
    // The runtime namespace includes methods related to the Qualcomm Hetcompute runtime and
    // the Qualcomm Hetcompute thread pool.
    namespace runtime
    {
        /** @addtogroup init_shutdown
        @{ */
        /**
            Starts up HetCompute SDK's runtime.
        
            Initializes Hetcompute internal data structures, tasks, schedulers, and
            thread pools. 
        */
        inline void init()
        {
            hetcompute::internal::init_impl(HetComputeApp::features::supportException());
        }

        /**
            Shuts down HetCompute SDK's runtime.
        
            Shuts down the runtime. It returns only when all running tasks
            have finished.
        */
        void shutdown();

        /** @} */ /* end_addtogroup init_shutdown */

        /**
           Callback function pointer.
        */
        typedef void (*callback_t)();
        
        /**
            Sets a callback function that HetCompute will invoke whenever it creates
            an internal runtime thread.
        
            Programmers should invoke this method before init() because init()
            may create threads. Programmers may reset the thread construction
            callback by using a nullptr as parameter to this method.
        
            @param fptr New callback function pointer.
        
            @return Previous callback function pointer or nullptr if called for the first time.
        */
        callback_t set_thread_created_callback(callback_t fptr);
        
        /**
            Sets a callback function that HetCompute will invoke whenever it destroys
            an internal runtime thread.
        
            Programmers should invoke this method before init() because init()
            may destroy threads. Programmers may reset the thread destruction
            callback by using a nullptr as parameter to this method.
            @param fptr New callback function pointer.
        
            @return Previous callback function pointer or nullptr if called for the first time.
        */
        callback_t set_thread_destroyed_callback(callback_t fptr);

        /**
            Returns a pointer to the function that HetCompute will invoke whenever
            it creates a new thread.
            @return Current callback function pointer or nullptr if none.
        */
        callback_t thread_created_callback();

        /**
            Returns a pointer to the function that HetCompute will invoke whenever
            it destroys a new thread.
            @return Current callback function pointer or nullptr if none.
        */
        callback_t thread_destroyed_callback();
    };  // namespace runtime

    namespace internal
    {
        class task_queue_list; // forward declaration for get_task_queue_list
        
        task_queue_list* get_task_queue_list();
    };
};  // namespace hetcompute
