#pragma once

#include <array>
#include <atomic>
#include <map>

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
                Record events relevant to work steal tree.
            */
            class pfor_logger : public logger_base
            {
            public:
                static const auto relaxed = hetcompute::mem_order_relaxed;

#ifndef HETCOMPUTE_USE_PFOR_LOGGER
                typedef std::false_type enabled;
#else
                typedef std::true_type enabled;
#endif

                // Initializes pfor_logger data structures
                static void init();

                // Destroys pfor_logger data structures
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
                static void count(events::ws_tree_new_slab&&)
                {
                    s_num_slabs.fetch_add(1, relaxed);
                }

                static void count(events::ws_tree_node_created&&)
                {
                    s_num_nodes.fetch_add(1, relaxed);
                }

                static void count(events::ws_tree_worker_try_own&&)
                {
                    s_num_own.fetch_add(1, relaxed);
                }

                static void count(events::ws_tree_worker_try_steal&&)
                {
                    s_num_steal.fetch_add(1, relaxed);
                }

                static void count(events::ws_tree_try_own_success&&)
                {
                    s_success_own.fetch_add(1, relaxed);
                }

                static void count(events::ws_tree_try_steal_success&&)
                {
                    s_success_steal.fetch_add(1, relaxed);
                }

                template <typename UnknownEvent>
                static void count(UnknownEvent&&)
                {
                }

                static std::atomic<size_t> s_num_nodes;
                static std::atomic<size_t> s_num_own;
                static std::atomic<size_t> s_num_slabs;
                static std::atomic<size_t> s_num_steal;
                static std::atomic<size_t> s_success_own;
                static std::atomic<size_t> s_success_steal;

                HETCOMPUTE_DELETE_METHOD(pfor_logger());
                HETCOMPUTE_DELETE_METHOD(pfor_logger(pfor_logger const&));
                HETCOMPUTE_DELETE_METHOD(pfor_logger(pfor_logger&&));
                HETCOMPUTE_DELETE_METHOD(pfor_logger& operator=(pfor_logger const&));
                HETCOMPUTE_DELETE_METHOD(pfor_logger& operator=(pfor_logger&&));

            }; // class pfor_logger

        }; // namespace hetcompute::internal::log
    };     // namespace hetcompute::internal
};         // namespace hetcompute
