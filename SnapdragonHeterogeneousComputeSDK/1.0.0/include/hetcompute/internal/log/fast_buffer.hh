#pragma once

#include <array>
#include <atomic>
#include <map>

#include <hetcompute/internal/log/events.hh>
#include <hetcompute/internal/log/objectid.hh>
#include <hetcompute/internal/log/loggerbase.hh>

#include <hetcompute/internal/util/debug.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace log
        {
            // buffer_entry represents an entry in the circular buffer
            // Each buffer_entry stores:
            //  - event counter
            //  - event id
            //  - id of the thread that executed the event
            //  - buffer for event handler
            //  - logger id
            class buffer_entry
            {
            public:
                // Maximum payload size.
                static constexpr size_t s_payload_size = 512;

                // Constructor
                HETCOMPUTE_MSC_IGNORE_BEGIN(4351);
                buffer_entry() : _logger_id(logger_base::logger_id::unknown), _event_count(0), _event_id(0), _thread_id(), _buffer()
                {
                }
                HETCOMPUTE_MSC_IGNORE_END(4351);

                // Resets entry with a new count, event id, and thread id.
                // Notice that we don't clean the payload, because we overwrite
                // it when we add a new event.
                void reset(event_context::counter_type c, event_id e, std::thread::id id, logger_base::logger_id lid)
                {
                    _event_count = c;
                    _event_id    = e;
                    _thread_id   = id;
                    _logger_id   = lid;
                }

                // Returns event id.
                event_context::counter_type get_event_count() const
                {
                    return _event_count;
                }

                // Returns event id.
                event_id get_event_id() const
                {
                    return _event_id;
                }

                // Returns thread id.
                std::thread::id get_thread_id() const
                {
                    return _thread_id;
                }

                // Returns pointer to buffer location
                void* get_buffer()
                {
                    return _buffer;
                }

                // Returns the logger id.
                logger_base::logger_id get_logger_id() const
                {
                    return _logger_id;
                }
            private:
                // ID of the logger
                logger_base::logger_id _logger_id;
                // Total counter
                event_context::counter_type _event_count;
                // ID of the event.
                event_id _event_id;
                // ID of the thread that caused the event
                std::thread::id _thread_id;
                // Event payload
                char _buffer[s_payload_size];
            }; // class buffer_entry

            // --------------------------------------
            // In-memory buffer.
            // All in-memory logger can use this fast_buffer.
            // --------------------------------------
            class fast_buffer
            {
            public:
                // Circular buffer size. Type and constant.
                typedef event_context::counter_type index_type;

// Maximum number of entries
#if defined(HETCOMPUTE_LOGGER_SIZE)
                static constexpr index_type s_size = HETCOMPUTE_LOGGER_SIZE;
#else
                static constexpr index_type s_size = 16384;
#endif

                // Type of the buffer
                typedef std::array<buffer_entry, s_size> log_array;

                // Get the buffer
                static log_array& get_default_buffer()
                {
                    return s_buffer;
                }

                // Get the pointer to the default buffer
                static fast_buffer* get_default_buffer_ptr()
                {
                    return s_default_buffer_ptr;
                }

                // Checks whether buffer is empty.
                static bool is_default_buffer_empty()
                {
                    buffer_entry& first_entry = s_default_buffer_ptr->s_buffer[0];
                    return first_entry.get_event_id() == events::null_event::get_id();
                }

                // Initialize the buffer
                static void init_default_buffer();

                // Returns a string with the thread ID.
                static std::string get_thread_name(std::thread::id thread_id);

                // Returns the first entry in the buffer. Remember that the buffer
                // is circular, so the first entry might not always be on pos 0
                static event_context::counter_type get_default_first_entry_pos()
                {
                    buffer_entry& first_entry            = s_default_buffer_ptr->s_buffer[0];
                    auto          lowest_event_count     = first_entry.get_event_count();
                    index_type    lowest_event_count_pos = 0;

                    // Find first entry in buffer
                    for (index_type i = 1; i < s_size; i++)
                    {
                        auto current_count = s_default_buffer_ptr->s_buffer[i].get_event_count();
                        if (current_count == 0)
                        {
                            break;
                        }

                        if (current_count < lowest_event_count)
                        {
                            lowest_event_count     = current_count;
                            lowest_event_count_pos = i;
                            break;
                        }
                    }

                    return lowest_event_count_pos;
                }

                // Dump the data from the default buffer
                static void dump_default_buffer();

                // Destructor
                ~fast_buffer();

            private:
                // Pointer to the default buffer
                static fast_buffer* s_default_buffer_ptr;

                // Default buffer, array of buffer_entries.
                static log_array s_buffer;

                // Thread map
                typedef std::map<std::thread::id, std::string> thread_map;
                static thread_map      s_threads;
                static std::thread::id s_main_thread;

            }; // class fast_buffer

        }; // namespace log
    };     // namespace internal
};         // namespace hetcompute
