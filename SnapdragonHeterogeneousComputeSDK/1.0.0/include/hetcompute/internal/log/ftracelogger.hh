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
               Logs Hetcompute events with ftrace using Qualcomm
               testframework format
            */
            class ftrace_logger : public logger_base
            {
            public:
#ifndef HETCOMPUTE_USE_FTRACE_LOGGER
                typedef std::false_type enabled;
#else
                typedef std::true_type enabled;
#endif

                // Initializes ftrace_logger data structures
                static void init();

                // Destroys ftrace_logger data structures
                static void shutdown();

                // Outputs buffer contents
                static void dump()
                {
                }

                // Pauses the logging infrastructure
                static void paused();

                // Resumes the logging infrastructure
                static void resumed();

                // Logs event. Because we want events as inlined as possible, the
                // implementation for the method is in ftracelogger.hh
                template <typename EVENT>
                static void log(EVENT&& e, event_context&)
                {
                    print(std::forward<EVENT>(e));
                }

            private:
                static void ftrace_write(const char* str);
                static void ftrace_write(const char* str, size_t len);

                static void print(events::group_canceled&& e);

                static void print(events::group_created&& e);

                static void print(events::group_destroyed&& e);

                static void print(events::group_reffed&& e);

                static void print(events::group_unreffed&& e);

                static void print(events::group_wait_for_ended&& e);

                static void print(events::task_after&& e);

                static void print(events::task_cleanup&& e);

                static void print(events::task_created&& e);

                static void print(events::task_destroyed&& e);

                static void print(events::task_finished&& e);

                static void print(events::task_executes&& e);

                static void print(events::task_sent_to_runtime&& e);

                static void print(events::task_reffed&& e);

                static void print(events::task_unreffed&& e);

                static void print(events::task_wait&& e);

                template <typename UNKNOWNEVENT>
                static void print(UNKNOWNEVENT&&)
                {
                }

                HETCOMPUTE_DELETE_METHOD(ftrace_logger());
                HETCOMPUTE_DELETE_METHOD(ftrace_logger(ftrace_logger const&));
                HETCOMPUTE_DELETE_METHOD(ftrace_logger(ftrace_logger&&));
                HETCOMPUTE_DELETE_METHOD(ftrace_logger& operator=(ftrace_logger const&));
                HETCOMPUTE_DELETE_METHOD(ftrace_logger& operator=(ftrace_logger&&));

            }; // class ftrace_logger

        }; // namespace hetcompute::internal::log
    };     // namespace hetcompute::internal
};         // namespace hetcompute
