#pragma once

#include <thread>

#include <hetcompute/internal/log/common.hh>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/memorder.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace log
        {
            /// Some operations are expensive. For example, geting the time
            /// of the day or the thread id. If we have multiple loggers,
            /// we want to avoid having each logger calling them for each
            /// event. This class takes care of that.

            /// The logging infrastructure creates an event_context object and
            /// passes it to all the loggers. If several loggers must know the
            /// thread id, only the first one that calls get_this_thread_id will
            /// actually call std::this_thread::get_id(). The rest will read the
            /// cached result.
            class event_context
            {
            public:
                typedef size_t   counter_type;
                typedef uint64_t time_type;

                /// Constructor. Does nothing
                event_context() : _count(0), _thread(), _when(0)
                {
                }

                /// Gets thread id of the calling thread. It only calls
                /// this_thread::get_id() once.
                std::thread::id get_this_thread_id()
                {
                    if (_thread == s_null_thread_id)
                    {
                        _thread = std::this_thread::get_id();
                    }
                    return _thread;
                }

                /// Gets time at which the first call to get_time happened.
                time_type get_time()
                {
                    if (_when == 0)
                    {
                        _when = hetcompute_get_time_now();
                    }
                    return _when;
                }

                /// Gets time at which the first call to get_time happened.
                counter_type get_count()
                {
                    if (_count == 0)
                    {
                        _count = s_global_counter.fetch_add(1, hetcompute::mem_order_relaxed);
                    }

                    return _count;
                }

            private:
                counter_type    _count;
                std::thread::id _thread;
                time_type       _when;

                static std::thread::id           s_null_thread_id;
                static std::atomic<counter_type> s_global_counter;

                // Disable all copying and movement, since other objects may have
                // pointers to the internal fields of this object.
                HETCOMPUTE_DELETE_METHOD(event_context(event_context const&));
                HETCOMPUTE_DELETE_METHOD(event_context(event_context&&));
                HETCOMPUTE_DELETE_METHOD(event_context& operator=(event_context const&));
                HETCOMPUTE_DELETE_METHOD(event_context& operator=(event_context&&));
            };

        }; // namespace hetcompute::internal::log
    };     // namespace hetcompute::internal
};         // namespace hetcompute
