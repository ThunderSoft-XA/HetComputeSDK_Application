#pragma once

#include <array>
#include <atomic>
#include <map>

#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/log/loggerbase.hh>
#include <hetcompute/internal/log/fast_buffer.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace log
        {
            // Logs the last s_size Hetcompute events in an in-memory buffer.
            // corelogger tries to be as fast as possible.
            class corelogger : public logger_base
            {
                // Circular buffer size. Type and constant.
                typedef event_context::counter_type index_type;
                // This is the capacity of the default buffer, see fast_buffer.hh
                static constexpr index_type s_size = fast_buffer::s_size;

                // Logger ID
                static const logger_id s_myloggerid = logger_id::corelogger;

            public:
#ifndef HETCOMPUTE_USE_CORELOGGER
                typedef std::false_type enabled;
#else
                typedef std::true_type enabled;
#endif

                // Initializes corelogger data structures
                static void init();

                // Destroys corelogger data structures
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
                // implementation for the method is in coreloggerevents.hh
                template <typename Event>
                static void log(Event&& e, event_context& context);

                // Transform entries logged into the default buffer to a string.
                static std::string to_string(void* str);

                // Returns a string with the thread ID.
                static std::string get_thread_name(std::thread::id thread_id);

                // Returns a name of the logger
                static std::string get_name()
                {
                    return "CORE_LOGGER";
                }

            private:
                // Pointer to the default buffer
                static fast_buffer* s_default_buf;

                // Thread map
                typedef std::map<std::thread::id, std::string> thread_map;
                static thread_map      s_threads;
                static std::thread::id s_main_thread;

                HETCOMPUTE_DELETE_METHOD(corelogger());
                HETCOMPUTE_DELETE_METHOD(corelogger(corelogger const&));
                HETCOMPUTE_DELETE_METHOD(corelogger(corelogger&&));
                HETCOMPUTE_DELETE_METHOD(corelogger& operator=(corelogger const&));
                HETCOMPUTE_DELETE_METHOD(corelogger& operator=(corelogger&&));

            }; // class corelogger
        }; // namespace log
    };     // namespace internal
};         // namespace hetcompute
