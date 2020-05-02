#pragma once

#include <array>
#include <list>

#include <hetcompute/internal/log/common.hh>
#include <hetcompute/internal/log/eventids.hh>
#include <hetcompute/internal/log/eventcontext.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace log
        {
            // Handlers base. Takes care of clearing
            struct user_handler_list_base
            {
                virtual void unregister_all() = 0;
                virtual ~user_handler_list_base()
                {
                }
            };

            template <typename Event>
            class user_handler_list;

            class user_handlers
            {
                typedef std::array<user_handler_list_base*, static_cast<size_t>(events::event_id_list::num_events)> handlers;

                static handlers* s_handlers;

                template <typename Event>
                static auto get_user_handler_list() -> user_handler_list<Event>*
                {
                    return static_cast<user_handler_list<Event>*>((*s_handlers)[Event::get_id()]);
                }

                template <typename Event>
                static auto create_user_handler_list() -> user_handler_list<Event>*
                {
                    auto list                      = new user_handler_list<Event>();
                    (*s_handlers)[Event::get_id()] = list;
                    return list;
                }

            public:
                static void init();

                static void destroy() { delete s_handlers; }

                template <typename Event>
                static void event_fired(Event&& event)
                {
                    auto list = get_user_handler_list<Event>();
                    if (!list)
                    {
                        return;
                    }

                    list->event_fired(std::forward<Event>(event));
                }

                template <typename Event>
                static handler_id add(typename Event::handler_func&& handler)
                {
                    auto list = get_user_handler_list<Event>();

                    if (list == nullptr)
                    {
                        list = create_user_handler_list<Event>();
                    }

                    return list->add(std::forward<typename Event::handler_func>(handler));
                }

                static void remove_all();

                // Unregister all event_handlers from an event
                template <typename Event>
                static void remove_all_for_event()
                {
                    auto list = get_user_handler_list<Event>();
                    if (!list)
                    {
                        return;
                    }

                    list->unregister_all();
                }

                // Unregister one event handler
                template <typename Event>
                static bool remove_one_for_event(handler_id id)
                {
                    auto list = get_user_handler_list<Event>();
                    if (!list)
                    {
                        HETCOMPUTE_ELOG("Invalid event handler id %d\n", id);
                        return false;
                    }

                    return list->unregister_handler(id);
                }
            };

            // Handlers
            template <typename Event>
            class user_handler_list : public user_handler_list_base
            {
            public:
                // handler function type
                typedef typename Event::handler_func handler_func;

            private:
                // Entry type
                struct entry
                {
                    handler_id   id;
                    handler_func func;

                    entry() : id(0), func(nullptr)
                    {
                    }

                    entry(handler_id i, handler_func f) : id(i), func(f)
                    {
                    }
                };

            public:
                // iterator type
                typedef typename std::list<entry>::iterator iterator;

                // Constructor.
                user_handler_list() : _entries()
                {
                }

                // Erase element pointed by iterator
                void erase(iterator it)
                {
                    _entries.erase(it);
                }

                // Checks whether the list is empty
                bool is_empty()
                {
                    return _entries.empty();
                }

                handler_id add(handler_func&& func)
                {
                    handler_id id;
                    if (_entries.empty())
                    {
                        id = 0;
                    }
                    else
                    {
                        id = _entries.back().id + 1;
                    }
                    _entries.emplace_back(id, std::forward<handler_func>(func));
                    return id;
                }

                void event_fired(Event&& event)
                {
                    event_context context;
                    for (auto& handler : _entries)
                    {
                        handler.func(std::forward<Event>(event), context);
                    }
                }

                bool unregister_handler(handler_id id)
                {
                    auto last = _entries.end();
                    for (auto it = _entries.begin(); it != last; ++it)
                    {
                        if ((*it).id == id)
                        {
                            _entries.erase(it); // delete the node in the list
                            return true;        // immediately return
                        }
                    }
                    HETCOMPUTE_ELOG("Invalid event handler id.\n");
                    return false;
                }

                // Removes all handlers from list
                void unregister_all()
                {
                    _entries.clear();
                }

            private:
                // Event handlers: a list of handler_entries
                std::list<entry> _entries;

                // Disable all copying and movement, since other objects may have
                // pointers to the internal fields of this object.
                HETCOMPUTE_DELETE_METHOD(user_handler_list(user_handler_list const&));
                HETCOMPUTE_DELETE_METHOD(user_handler_list(user_handler_list&&));
                HETCOMPUTE_DELETE_METHOD(user_handler_list& operator=(user_handler_list const&));
                HETCOMPUTE_DELETE_METHOD(user_handler_list& operator=(user_handler_list&&));
            };

        }; // namespace hetcompute::internal::log
    };     // namespace hetcompute::internal
};         // namespace hetcompute
