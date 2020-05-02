#pragma once

#include <array>
#include <atomic>

#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/log/loggerbase.hh>
#include <hetcompute/internal/log/objectid.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace log
        {
            /**
               Counts Hetcompute events.
            */
            class event_counter_logger : public logger_base
            {
            public:
#ifndef HETCOMPUTE_USE_EVENT_COUNTER_LOGGER
                typedef std::false_type enabled;
#else
                typedef std::true_type enabled;
#endif

                // Initializes event_counter_logger data structures
                static void init();

                // Destroys event_counter_logger data structures
                static void shutdown();

                // Outputs buffer contents
                static void dump();

                // Logging infrastructure has been paused
                static void paused()
                {
                }

                // Logging infrastructure has been resumed
                static void resumed()
                {
                }

                // Logs event. Because we want events as inlined as possible, the
                // implementation for the method is in event_counter_loggerevents.hh
                template <typename Event>
                static void log(Event&& e, event_context&)
                {
                    count(std::forward<Event>(e));
                }

            private:
                static void count(events::task_created&&) { s_num_tasks.fetch_add(1, hetcompute::mem_order_relaxed); }

                static void count(events::runtime_enabled&& e)
                {
                    s_num_exec_ctx       = e.get_num_exec_ctx();
                    s_num_tasks_executed = new size_t[s_num_exec_ctx];
                    std::fill(s_num_tasks_executed, s_num_tasks_executed + s_num_exec_ctx, 0);
                }

                static void count(events::task_queue_list_created&& e)
                {
                    s_num_qs           = e.get_num_queues();
                    s_foreign_queue_id = e.get_foreign_queue();
                    s_main_queue_id    = e.get_main_queue();
                    s_device_queues_id = e.get_device_queues();
                    s_num_tasks_stolen = new std::atomic<size_t>[s_num_qs];
                    std::fill(s_num_tasks_stolen, s_num_tasks_stolen + s_num_qs, 0);
                }

                static void count(events::task_executes&& e)
                {
                    if (e.is_blocking())
                    {
                        s_num_blocking_tasks_exec.fetch_add(1, hetcompute::mem_order_relaxed);
                    }
                    else if (e.is_gpu())
                    {
                        s_num_gpu_tasks_exec.fetch_add(1, hetcompute::mem_order_relaxed);
                    }
                    else if (e.is_inline())
                    {
                        // Needs to be atomic since read/written by main/foreign threads
                        s_num_inline_tasks_exec.fetch_add(1, hetcompute::mem_order_relaxed);
                    }
                    else
                    {
                        // As long as ids are different, no need for atomic
                        auto id = e.get_tid();
                        s_num_tasks_executed[id]++;
                    }
                }

                static void count(events::task_stolen&& e)
                {
                    // Multiple threads may steal from the same q at the same time, so atomic.
                    auto qid = e.get_tq_from();
                    s_num_tasks_stolen[qid].fetch_add(1, hetcompute::mem_order_relaxed);
                }

                static void count(events::group_created&&) { s_num_groups.fetch_add(1, hetcompute::mem_order_relaxed); }

                template <typename UnknownEvent>
                static void count(UnknownEvent&&)
                {
                }

                static std::atomic<size_t> s_num_tasks;
                static std::atomic<size_t> s_num_groups;
                static size_t              s_num_exec_ctx;
                static size_t              s_num_qs;
                static size_t              s_foreign_queue_id;
                static size_t              s_main_queue_id;
                static size_t              s_device_queues_id;
                static size_t*             s_num_tasks_executed; // Map from thread id to num tasks
                // Number of gpu tasks executed
                static std::atomic<size_t> s_num_gpu_tasks_exec;
                // Number of blocking tasks executed
                static std::atomic<size_t> s_num_blocking_tasks_exec;
                // Number of tasks executed inline by the main/foreign threads
                static std::atomic<size_t>  s_num_inline_tasks_exec;
                static std::atomic<size_t>* s_num_tasks_stolen; // Map from qid to num tasks

                HETCOMPUTE_DELETE_METHOD(event_counter_logger());
                HETCOMPUTE_DELETE_METHOD(event_counter_logger(event_counter_logger const&));
                HETCOMPUTE_DELETE_METHOD(event_counter_logger(event_counter_logger&&));
                HETCOMPUTE_DELETE_METHOD(event_counter_logger& operator=(event_counter_logger const&));
                HETCOMPUTE_DELETE_METHOD(event_counter_logger& operator=(event_counter_logger&&));

            }; // class event_counter_logger

        }; // namespace hetcompute::internal::log
    };     // namespace hetcompute::internal
};         // namespace hetcompute
