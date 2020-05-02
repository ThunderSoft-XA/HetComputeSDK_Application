/** @file userlogapi.hh */
#pragma once

#include <hetcompute/internal/log/events.hh>
#include <hetcompute/internal/log/loggerbase.hh>
#include <hetcompute/internal/log/userhandlers.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace log
        {
            // -----------------------------------------------------------------
            // Hetcompute INTERNAL API
            // -----------------------------------------------------------------
            /**
              Register user event handlers for a certain Event. This handler will be
              invoked when the Event happens.

              This method returns the handler_id (size_t) for an event_handler. The
              handler_id represents current event_handler's position in the event
              handler's queue for each specific Event.

              This method is a template, so a type of Event is needed to be passed in.

              @param Event: any event type available in events.hh

              @param handler: is the event_handler to be registered. It should have the
              format of [](Event&&, Context&&){...}.
            */
            template <typename Event>
            handler_id register_event_handler(typename Event::handler_func&& handler)
            {
                typedef typename Event::handler_func handler_func;
                return user_handlers::add<Event>(std::forward<handler_func>(handler));
            }

            /**
               Unregister event_handler with handler_id '@param id' for a certain Event.

               The method returns true if unregister succeed, and returns false otherwise.

               Specify an Event type is required for template.

               @param id should be a positive integer that is smaller than the total number
               of event_handlers for this certain Event.
            */
            template <typename Event>
            inline bool unregister_event_handler(handler_id id)
            {
                return user_handlers::remove_one_for_event<Event>(id);
            }

            /**
               Unregister all handlers for a certain Event.

               This method does not have return values and takes in zero param.
               Specify an Event type is required for template.
            */
            template <typename Event>
            inline void unregister_all_event_handlers()
            {
                user_handlers::remove_all_for_event<Event>();
            }

            /**
               Unregister all handlers for all Events.

               This method does not have return values nor params. It unregisters all
               event_handlers that has been registered for all the Events.
            */
            inline void unregister_all_event_handlers() { user_handlers::remove_all(); }
            /**
               Fire a user defined event.

               This method is used to insert user logging information to the default
               internal logging buffer. This method allows users to see interleavings
               between internal loggers and anywhere in a program that this function
               is invoked.

               @param func is a callable function pointer that takes an arbitrary
               parameter and returns a string. The return value will be inserted into
               the default shared logging buffer.

               @param p is a pointer to the object that will be passed into func as the
               parameter.
            %
               This method has no return value.
            */
            template <typename UserType>
            inline void add_log_entry(UserType& p, std::function<std::string(UserType*)> func)
            {
                event_context context;
                auto          count = context.get_count();
                auto          pos   = count % fast_buffer::s_size;
                auto&         entry = fast_buffer::get_default_buffer()[pos];

                entry.reset(count, events::user_log_event<UserType>::get_id(), context.get_this_thread_id(), logger_base::logger_id::userlogger);

                static_assert(sizeof(events::user_log_event<UserType>) <= buffer_entry::s_payload_size,
                              "Userlog is larger than entry payload.");

                events::user_log_event<UserType>* loc = reinterpret_cast<events::user_log_event<UserType>*>(entry.get_buffer());
                new (entry.get_buffer()) events::user_log_event<UserType>(std::forward<UserType>(p));
                loc->func = func;
            }

            /**
               Fire a user defined event.

               This method is used to insert user logging information to the default
               internal logging buffer. This method allows users to see interleavings
               between internal loggers and anywhere in a program that this function
               is invoked.

               @param s is string that will be inserted into the default shared logging
               buffer.

               This method has no return value.
            */
            inline void add_log_entry(std::string& s)
            {
                event_context context;

                auto  count = context.get_count();
                auto  pos   = count % fast_buffer::s_size;
                auto& entry = fast_buffer::get_default_buffer()[pos];

                entry.reset(count, events::user_string_event::get_id(), context.get_this_thread_id(), logger_base::logger_id::userlogger);

                static_assert(sizeof(s) <= buffer_entry::s_payload_size, "Userlog is larger than entry payload.");

                std::strcpy(reinterpret_cast<char*>(entry.get_buffer()), s.c_str());
            }

        }; // namespace log
    };     // namespace internal
};         // namespace hetcompute
