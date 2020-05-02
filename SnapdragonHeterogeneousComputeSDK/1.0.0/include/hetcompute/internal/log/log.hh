#pragma once

// Enable the ftrace logger only for Android and Linux
#if defined(__ANDROID__) || defined(__linux__)
#include <hetcompute/internal/log/ftracelogger.hh>
#endif

#include <hetcompute/internal/log/corelogger.hh>
#include <hetcompute/internal/log/eventcounterlogger.hh>
#include <hetcompute/internal/log/infrastructure.hh>
#include <hetcompute/internal/log/pforlogger.hh>
#include <hetcompute/internal/log/schedulerlogger.hh>
#include <hetcompute/internal/log/userhandlers.hh>
#include <hetcompute/internal/log/userlogapi.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace log
        {
// List of supported loggers. Please keep corelogger first because we
// want it to be the first one.
// ToDo: Transition to a typelist
#if defined(__ANDROID__) || defined(__linux__)
            typedef infrastructure<event_counter_logger, ftrace_logger, pfor_logger, schedulerlogger> loggers;
#else
            typedef infrastructure<event_counter_logger, pfor_logger, schedulerlogger> loggers;
#endif

            typedef loggers::enabled  enabled;
            typedef loggers::group_id group_id;
            typedef loggers::task_id  task_id;

            void init();
            void shutdown();

            void pause();
            void resume();
            void dump();

// Fire an event
#ifdef HETCOMPUTE_LOG_FIRE_EVENT
            template <typename Event, typename... Args>
            inline void fire_event(Args... args)
            {
                auto e = Event(std::forward<Args>(args)...);
                loggers::event_fired(e);
                user_handlers::event_fired(std::forward<Event>(e));
            }
#else
            template <typename Event, typename... Args>
            inline void fire_event(Args...)
            {
            }
#endif

        }; // namespace log
    };     // namespace internal
};         // namespace hetcompute
