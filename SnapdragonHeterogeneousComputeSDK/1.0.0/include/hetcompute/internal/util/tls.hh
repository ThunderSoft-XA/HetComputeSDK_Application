#pragma once

#include <atomic>
#include <cstdint>
#include <hetcompute/internal/scheduler/task_domain.hh>
#include <hetcompute/internal/scheduler/thread_type.hh>
#include <hetcompute/internal/util/tlsptr.hh>

namespace hetcompute
{
    namespace internal
    {
        class task;

        namespace tls
        {
            // We put thread specific information here.  We try to make sure that
            // every thread we see has this information.  Foreign threads (threads
            // that push a task that HetCompute does not control) may not have any
            // information.
            //
            // The class is aranged this way so you can get the pointer once and
            // reuse it locally with no penalty.
            class info
            {

#if !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
                // ID generator for thread allocators
                static size_t get_next_allocator_id()
                {
                    static std::atomic<size_t> s_allocator_id_counter(0);
                    auto                       id = s_allocator_id_counter.fetch_add(1);
                    return id;
                }
#endif // !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)

            public:
                /**
                 * Default Constructor
                 *
                 * Initializes the thread as foreign and belonging to task_domain::cpu_all.
                 *
                */
                info()
                    : _task(nullptr),
                      _thread_id(),
                      _thread_type(internal::thread_type::foreign),
                      _task_domain(internal::task_domain::cpu_all)
#if !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
                      ,
                      _allocator_id(get_next_allocator_id())
#endif // !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
                {
                }

                HETCOMPUTE_DEFAULT_METHOD(~info());
                HETCOMPUTE_DELETE_METHOD(info(const info&));
                HETCOMPUTE_DELETE_METHOD(info& operator=(const info&));

                /**
                 * Returns pointer to the task running in the current thread.
                 *
                 * @return internal::task* to current task.
                 */
                task* current_task() const
                {
                    return _task;
                }

                /**
                 * Set task as current task and returns pointer to the task
                 * running previously in the thread.
                 *
                 * @param t Pointer to the task that will execute in the current thread.
                 *          Can be nullptr.
                 *
                 * @return internal::task* to the task running previously in the thread.
                 *          Can be nullptr.
                */
                task* set_current_task(task* t)
                {
                    auto prev = _task;
                    _task     = t;
                    return prev;
                }

                /**
                 * Returns thread id for the current thread.
                 *
                 * @return uintptr_t with the id for the current thread.
                */
                uintptr_t thread_id() const
                {
                    return _thread_id;
                }

                /**
                 * Sets thread id for current thread.
                 *
                 * @param tid Thread id.
                 *
                */
                void set_thread_id(uintptr_t tid)
                {
                    _thread_id = tid;
                }

                /**
                 * Checks whether the calling thread is the main thread.
                 * From HETCOMPUTE's point of view, the main thread is the
                 * one that calls runtime::init. There is only
                 * one main thread.
                 *
                 * @return
                 *   true  -- The thread is the main thread.
                 *   false -- The thread is not the main thread.
                */
                bool is_main_thread() const
                {
                    return _thread_type == internal::thread_type::main;
                }

                /**
                 * Sets the calling thread as a main thread.
                 *
                 * Can only be called once. It will cause a fatal error
                 * if called twice.
                 *
                 * From HETCOMPUTE's point of view, the main thread is the
                 * one that calls runtime::init. There is only
                 * one main thread.
                */
                void set_main_thread();
                
                /**
                 * Resets the thread type to foreign.
                 *
                 * Also resets the main thread flag if the calling thread's type is main.
                 *
                */
                void reset_thread_type();

                /**
                 * Checks whether the current thread is a foreign thread.
                 *
                 * We call foreign threads those threads that are not
                 * created by HetCompute and are not the main thread. By default all
                 * threads are foreign threads.
                 *
                 * @return
                 *   true  -- The current thread is a foreign thread.
                 *   false -- The current thread is not a foreign thread.
                */
                bool is_foreign_thread() const
                {
                    return _thread_type == internal::thread_type::foreign;
                }

                /**
                 * Checks whether current thread is a HetCompute thread.
                 *
                 * We call HetCompute threads those that are created by HETCOMPUTE.
                 *
                 * @return
                 *   true  -- the current thread is a HetCompute thread.
                 *   false -- the current thread is not a HetCompute thread.
                */
                bool is_hetcompute_thread() const
                {
                    return _thread_type == internal::thread_type::hetcompute;
                }

                /**
                 * Sets current thread as HetCompute thread.
                 *
                 * We call HetCompute threads those that are created by HETCOMPUTE.
                */
                void set_hetcompute_thread();

                /**
                 * Returns task domain for current thread.
                 *
                 * @return
                 *   task_domain of calling thread.
                */
                task_domain get_task_domain()
                {
                    return _task_domain;
                }

                /**
                 * Sets task_domain for current thread.
                 *
                 * @param td Task domain.
                */
                void set_task_domain(task_domain td)
                {
                    _task_domain = td;
                }

#if !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
                size_t allocator_id() const
                {
                    return _allocator_id;
                }
#endif // !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)

            private:
                internal::task*       _task;
                uintptr_t             _thread_id;
                internal::thread_type _thread_type;
                internal::task_domain _task_domain;

#if !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
                size_t                _allocator_id;
#endif // !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
            };

            // Prototype for the TLS pointer
            extern tlsptr<info, storage::owner>* g_tls_info;

            // Static methods for single use
            info* init();
            void  init_tls_info();
            void  shutdown_tls_info();
            void  error(std::string msg, const char* filename, int lineno, const char* funcname);

            // This will get you the pointer so you can use it locally
            inline info* get()
            {
                auto ti = g_tls_info->get();
                if (ti)
                {
                    return ti;
                }
                return init();
            }
        }; // namespace tls

        inline uintptr_t thread_id()
        {
            auto ti = tls::get();
            return ti->thread_id();
        }

        inline void set_main_thread()
        {
            auto ti = tls::get();
            ti->set_main_thread();
        }
        
        inline void reset_thread_type()
        {
            auto ti = tls::get();
            ti->reset_thread_type();
        }

        inline void set_hetcompute_thread()
        {
            auto ti = tls::get();
            ti->set_hetcompute_thread();
        }

        inline void set_task_domain(hetcompute::internal::task_domain td)
        {
            auto ti = tls::get();
            ti->set_task_domain(td);
        }

    }; // namespace internal
};     // namespace hetcompute
