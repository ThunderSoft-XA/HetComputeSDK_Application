#pragma once

#include <array>

#include <hetcompute/internal/compat/compat.h>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/log/common.hh>
#include <hetcompute/internal/log/eventids.hh>
#include <hetcompute/internal/log/eventcontext.hh>

namespace hetcompute
{
    namespace internal
    {
        // forward declarations
        class task;
        class group;

        namespace log
        {
            ///  The following templates are used as boilerplates for all the
            ///  possible events in the system.
            ///
            ///   All events MUST inherit from base_event.
            template <typename Event>
            class base_event
            {
            public:
                // Get the event id
                static constexpr event_id get_id()
                {
                    return Event::s_id;
                }

                // Define handler function type
                typedef std::function<void(Event&&, event_context&)> handler_func;
            };

            ///  Many events only need to store a pointer to the object that caused
            ///  it (i.e: task, group). Therefore, we use this base class template
            ///  to represent them.
            template <typename ObjectType, typename Event>
            struct single_object_event : public base_event<Event>
            {
                explicit single_object_event(ObjectType* object) : _object(object)
                {
                }

                // Returns object that caused event.
                ObjectType* get_object() const
                {
                    return _object;
                }
            private:
                ObjectType* const _object;
            };

            /// Base class for events that involve just one group.
            template <typename Event>
            struct single_group_event : public single_object_event<group, Event>
            {
                explicit single_group_event(group* g) : single_object_event<group, Event>(g)
                {
                }

                // Returns group that caused the event.
                group* get_group() const
                {
                    return single_object_event<group, Event>::get_object();
                }
            };

            /// Base class for group_reffed, group_unreffed events.
            template <typename Event>
            struct group_ref_count_event : public single_group_event<Event>
            {
                group_ref_count_event(group* g, size_t count) : single_group_event<Event>(g), _count(count)
                {
                }

                // Returns new refcount for the group
                size_t get_count() const
                {
                    return _count;
                }

            private:
                const size_t _count;
            };

            /// Base class for events that involve just one task.
            template <typename Event>
            struct single_task_event : public single_object_event<task, Event>
            {
                explicit single_task_event(task* g) : single_object_event<task, Event>(g)
                {
                }

                // Returns task that caused the event
                task* get_task() const
                {
                    return single_object_event<task, Event>::get_object();
                }
            };

            /// Base class for class_reffed, class_unreffed events
            template <typename Event>
            struct task_ref_count_event : public single_task_event<Event>
            {
                task_ref_count_event(task* t, size_t count) : single_task_event<Event>(t), _count(count)
                {
                }

                // Returns the value of the new ref_count for the tasks.
                size_t get_count() const
                {
                    return _count;
                }
            private:
                const size_t _count;
            };

            /// Base class for events that involve two tasks.
            template <typename Event>
            struct dual_task_event : public single_task_event<Event>
            {
                dual_task_event(task* t1, task* t2) : single_task_event<Event>(t1), _other(t2)
                {
                }

                // Returns second task involved in event
                task* get_other_task() const
                {
                    return _other;
                }
            private:
                task* const _other;
            };

            ///  Events related to ref_counting need to store a pointer to the object
            ///  and a counter.
            template <typename Event>
            struct object_ref_count_event : public base_event<Event>
            {
                object_ref_count_event(void* o, size_t count) : _o(o), _count(count)
                {
                }

                // Returns object whose refcount changed
                void* get_object() const
                {
                    return _o;
                }

                // Returns new refcount
                size_t get_count() const
                {
                    return _count;
                }
            private:
                void* const  _o;
                const size_t _count;
            };

        }; // hetcompute::internal::log
    };     // hetcompute::internal
};         // hetcompute
