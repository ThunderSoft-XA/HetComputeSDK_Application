#pragma once

#include <hetcompute/internal/log/events.hh>
#include <hetcompute/internal/log/objectid.hh>

#include <hetcompute/internal/util/debug.hh>
/**
   All loggers must inherit from logger_base.

   The class is empty now, but that might change
   in the future.
 */
namespace hetcompute
{
    namespace internal
    {
        namespace log
        {
            class logger_base
            {
            public:
                virtual ~logger_base()
                {
                }

                enum class logger_id : int8_t
                {
                    unknown         = -1,
                    corelogger      = 0,
                    schedulerlogger = 1,
                    userlogger      = 2
                };
            }; // class logger_base

        }; // namespace log
    };     // namespace internal
};         // namespace hetcompute



/*
Use the following code as a blueprint for your own logger.

#pragma once

#include "loggerbase.hh"
#include "events.hh"

namespace hetcompute
{
    namespace internal
    {
        namespace log
        {
            struct my_logger : public logger_base
            {
#ifndef HETCOMPUTE_USE_MYLOGGER
                typedef std::false_type enabled;
#else
                typedef std::true_type enabled;
#endif

                static void log(task_destroyed&& event, event_context& context) { ... }
                template <typename UnknownEvent>
                static void log(UnknownEvent&& e, event_context& context)
                {
                    // do nothing
                }

                static void init() {}
                static void shutdown() {}
                Logging infrastructure has been paused static void paused(){};

                // Logging infrastructure has been resumed
                static void resumed(){};

                HETCOMPUTE_DELETE_METHOD(mylogger());
                HETCOMPUTE_DELETE_METHOD(mylogger(mylogger const&));
                HETCOMPUTE_DELETE_METHOD(mylogger(mylogger&&));
                HETCOMPUTE_DELETE_METHOD(mylogger& operator=(mylogger const&));
                HETCOMPUTE_DELETE_METHOD(mylogger& operator=(mylogger&&));
            };

        }; // namespace hetcompute::internal::log
    };     // namespace hetcompute::internal
};         // namespace hetcompute
*/
