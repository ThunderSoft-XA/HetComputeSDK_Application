#pragma once

#include <utility>

#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/templatemagic.hh>

#include "events.hh"
#include "loggerbase.hh"
#include "fast_buffer.hh"
#include "objectid.hh"

namespace hetcompute
{
    namespace internal
    {
        namespace log
        {
            /**
               These templates check whether any logger is enabled.
               If that's the case, then value is true. Otherwise, value
               is false
            */
            template <typename... LS>
            struct any_logger_enabled;
            template <>
            struct any_logger_enabled<> : public std::integral_constant<bool, false>
            {
            };

            template <typename L1, typename... LS>
            struct any_logger_enabled<L1, LS...>
                : public std::integral_constant<bool, (L1::enabled::value) ? true : any_logger_enabled<LS...>::value>
            {
            };

            /**
               These templates are used to call log(), init(), and shutdown()
               on the enabled loggers
             */
            template <typename... T>
            struct method_dispatcher;

            template <>
            struct method_dispatcher<>
            {
                template <typename Event>
                static void log(Event&&, event_context&)
                {
                }

                static void init()
                {
                }

                static void shutdown()
                {
                }

                static void dump()
                {
                }

                static void pause()
                {
                }

                static void resume()
                {
                }
            };

            template <typename L1, typename... LS>
            struct method_dispatcher<L1, LS...>
            {
            private:
                template <typename Event>
                static void _log(Event&& e, event_context& context, std::true_type)
                {
                    L1::log(std::forward<Event>(e), context);
                }

                template <typename Event>
                static void _log(Event&&, event_context&, std::false_type)
                {
                }

            public:
                template <typename Event>
                static void log(Event&& e, event_context& context)
                {
                    _log(std::forward<Event>(e), context, typename L1::enabled());
                    method_dispatcher<LS...>::log(std::forward<Event>(e), context);
                }

                static void init()
                {
                    static_assert(std::is_base_of<logger_base, L1>::value,
                                  "\nAll hetcompute loggers must inherit from logger_base."
                                  "\nPlease read loggerbase.hh to learn how to "
                                  "write a logger");

                    // We need to initialize this because of user events
                    hetcompute::internal::log::fast_buffer::init_default_buffer();

                    if (L1::enabled::value)
                    {
                        L1::init();
                    }
                    method_dispatcher<LS...>::init();
                }

                static void shutdown()
                {
                    if (L1::enabled::value)
                    {
                        L1::shutdown();
                    }
                    method_dispatcher<LS...>::shutdown();
                }

                static void dump()
                {
                    if (L1::enabled::value)
                    {
                        L1::dump();
                    }
                    method_dispatcher<LS...>::dump();
                }

                static void pause()
                {
                    if (L1::enabled::value)
                    {
                        L1::paused();
                    }
                    method_dispatcher<LS...>::pause();
                }

                static void resume()
                {
                    if (L1::enabled::value)
                    {
                        L1::resumed();
                    }
                    method_dispatcher<LS...>::resume();
                }
            };

            /**
               This template represents the Hetcompute logging infrastructure. Each of
               the template parameters is a different logger.
             */
            template <typename L1, typename... LN>
            class infrastructure
            {
            private:
                // std::integral_constant<bool, true> if at least one of the loggers
                // in the system is enabled. Otherwise, std::integral_constant<bool,
                // false>.
                typedef typename any_logger_enabled<L1, LN...>::integral_constant any_logger_enabled;

                // Loggers in the system. We could have use a type list of some
                // sort, but this strategy worked and I felt it was just ok.
                typedef method_dispatcher<L1, LN...> loggers;

                // The logging infrastructure starts in uninitialized state. It transitions
                // to active on init(). It transitions to paused on pause() and returns
                // to active on resume(). Finally, it transitions to FINISHED
                // on shutdown().
                enum class status : unsigned
                {
                    uninitialized,
                    active,
                    paused,
                    finished
                };
                static status s_status;

                // Called by event(Event&& e) when no logger is enabled, thus making
                // sure that the compiler can optimize everything away if all loggers
                // are disabled.
                template <typename Event>
                static void _event(Event&&, std::false_type)
                {
                }

                // Called by event(Event&& e) when at least one logger is enabled.
                // Returns immediately if the logging infrastructure is not in
                // active mode. This means that we have a dynamic check for
                // each event logging.
                template <typename Event>
                static void _event(Event&& e, std::true_type)
                {
                    if (s_status != status::active)
                    {
                        return;
                    }

                    event_context context;
                    loggers::log(std::forward<Event>(e), context);
                }

            public:
                // This template allows us to choose the type of object_id that
                // we'll use. If no logger is enabled, we'll use
                // null_log_object_id. Otherwise, we'll use seq_log_object_id.
                typedef typename std::conditional<any_logger_enabled::value, seq_object_id<task>, null_object_id<task>>::type task_id;

                typedef typename std::conditional<any_logger_enabled::value, seq_object_id<group>, null_object_id<group>>::type group_id;

                typedef any_logger_enabled enabled;
                // Initializes logging infrastructure by calling init() on all
                // active loggers.  It causes a transition to active state.  It
                // should only be called once. Subsequent calls to init() will be
                // ignored.
                static void init()
                {
                    if (any_logger_enabled::value == false || s_status != status::uninitialized)
                    {
                        return;
                    }

                    static_assert(duplicated_types<L1, LN...>::value == false, "\nDuplicated logger types in logging infrastructure.");

                    loggers::init();

                    s_status = status::active;
                }

                // Shuts the logging infrastructure down by calling shutdown() on
                // all active loggers.  It causes a transition to paused state.  It
                // should only be called once. Subsequent calls to shutdown() will
                // be ignored.
                static void shutdown()
                {
                    if (any_logger_enabled::value == false || s_status == status::uninitialized || s_status == status::paused)
                    {
                        return;
                    }
                    fast_buffer::dump_default_buffer();
                    loggers::shutdown();
                    s_status = status::paused;
                }

                // Pauses logging. Causes a transtition to paused state.
                static void pause()
                {
                    if (any_logger_enabled::value == false || s_status == status::paused)
                    {
                        return;
                    }
                    s_status = status::paused;
                    loggers::pause();
                }

                // Resumes logging.  Causes a transtition to LOGGING state.
                static void resume()
                {
                    if (any_logger_enabled::value == false || s_status == status::active)
                    {
                        return;
                    }

                    s_status = status::active;
                    loggers::resume();
                }

                // Returns true if the logging has been initialized but not shut
                // down yet.
                static bool is_initialized()
                {
                    return (s_status == status::active) || (s_status == status::paused);
                }

                // Returns true if the logging is in active state.
                static bool is_active()
                {
                    return (s_status == status::active);
                }

                // Returns true if the logging is in paused state.
                static bool is_paused()
                {
                    return (s_status == status::paused);
                }

                static void dump()
                {
                    loggers::dump();
                }

                // Logs event e.
                template <typename Event>
                static void event_fired(Event&& e)
                {
                    _event(std::forward<Event>(e), any_logger_enabled());
                }

                // Disable all construction, copying and movement
                HETCOMPUTE_DELETE_METHOD(infrastructure());
                HETCOMPUTE_DELETE_METHOD(infrastructure(infrastructure const&));
                HETCOMPUTE_DELETE_METHOD(infrastructure(infrastructure&&));
                HETCOMPUTE_DELETE_METHOD(infrastructure& operator=(infrastructure const&));
                HETCOMPUTE_DELETE_METHOD(infrastructure& operator=(infrastructure&&));
            }; // class infrastructure

            // Static members initialization
            template <typename L1, typename... LN>
            typename infrastructure<L1, LN...>::status infrastructure<L1, LN...>::s_status = infrastructure::status::uninitialized;

        }; // hetcompute::internal::log
    };     // hetcompute::internal
};         // hetcompute
