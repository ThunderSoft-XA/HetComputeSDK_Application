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
            // Schedulerlogger records events caused by scheduler.
            // For now it only has events::task_stolen.
            class schedulerlogger : public logger_base
            {
                // Circular buffer size. Type and constant.
                typedef event_context::counter_type index_type;
                // Capacity of default buffer, refer to fast_buffer.hh
                static constexpr index_type s_size = fast_buffer::s_size;

                // Logger ID
                static const logger_id s_myloggerid = logger_id::schedulerlogger;

            public:
#ifndef HETCOMPUTE_USE_SCHEDULER_LOGGER
                typedef std::false_type enabled;
#else
                typedef std::true_type enabled;
#endif

                // Initializes schedulerlogger data structures
                static void init();

                // Destroys schedulerlogger data structures
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
                // implementation for the method is in schedulerloggerevents.hh
                template <typename Event>
                static void log(Event&& e, event_context& context);

                // Transform entries into strings
                static std::string to_string(void* str);

                // Returns a string with the thread ID.
                static std::string get_thread_name(std::thread::id thread_id);

                // Returns a name of the logger
                static std::string get_name()
                {
                    return "SCHD_LOGGER";
                }

            private:
                // Pointer to the default buffer, actual buffer: s_default_buf->s_buffer
                static fast_buffer* s_default_buf;

                // windows VS complains about unreferenced formal parameter of e
                static void logging(events::task_stolen&& e, event_context& context)
                {
                    auto  count = context.get_count();
                    auto  pos   = count % s_size;
                    auto& entry = s_default_buf->get_default_buffer()[pos];
                    auto  eid   = e.get_id();

                    entry.reset(count, eid, context.get_this_thread_id(), s_myloggerid);
                    std::strcpy(reinterpret_cast<char*>(entry.get_buffer()), "Task stolen\t");
                }

                template <typename UnknownEvent>
                static void logging(UnknownEvent&&, event_context&)
                {
                }

                // Returns the first entry in the buffer. Remember that the buffer
                // is circular, so the first entry might not always be on pos 0
                static event_context::counter_type get_first_entry_pos();

                // Thread map
                typedef std::map<std::thread::id, std::string> thread_map;
                static thread_map      s_threads;
                static std::thread::id s_main_thread;

                HETCOMPUTE_DELETE_METHOD(schedulerlogger());
                HETCOMPUTE_DELETE_METHOD(schedulerlogger(schedulerlogger const&));
                HETCOMPUTE_DELETE_METHOD(schedulerlogger(schedulerlogger&&));
                HETCOMPUTE_DELETE_METHOD(schedulerlogger& operator=(schedulerlogger const&));
                HETCOMPUTE_DELETE_METHOD(schedulerlogger& operator=(schedulerlogger&&));

            }; // class schedulerlogger

            template <typename Event>
            inline void schedulerlogger::log(Event&& event, event_context& context)
            {
                logging(std::forward<Event>(event), context);
            }

        }; // namespace hetcompute::internal::log
    };     // namespace hetcompute::internal
};         // namespace hetcompute
