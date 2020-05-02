#pragma once

#include <atomic>
#include <thread>

#include <hetcompute/internal/log/eventsbase.hh>
#include <hetcompute/internal/storage/schedulerstorage.hh>


namespace hetcompute
{
    namespace internal
    {
        class group; // only forward declaration should be enough
        namespace log
        {
            namespace events
            {
                struct null_event : public base_event<null_event>
                {
                    static const event_id s_id = static_cast<event_id>(event_id_list::null_event);
                };

                struct unknown_event : public base_event<unknown_event>
                {
                    static const event_id s_id = static_cast<event_id>(event_id_list::unknown_event);
                };

                // ----------------------------------------------------------------------
                // user_log_event_base, since this event does not need event_handlers defined
                // by the users, and since we want the size of the struct to be as small as
                // possible, this user_log_event_base does not inherit from base_event.
                // ----------------------------------------------------------------------
                struct user_log_event_base
                {
                    virtual std::string to_string() = 0;
                    virtual ~user_log_event_base()
                    {
                    }

                    static const event_id s_id = static_cast<event_id>(event_id_list::user_log_event_base);
                    // Get the event id
                    static constexpr event_id get_id()
                    {
                        return s_id;
                    }
                };

                // user_log_event
                template <typename UserType>
                struct user_log_event : public user_log_event_base
                {
                    typedef std::function<std::string(UserType* p)> U_Func;
                    UserType                                        param; // parameter
                    U_Func                                          func;  // function

                    explicit user_log_event(UserType p) : param(p)
                    {
                    }

                    // invoke the function
                    std::string to_string()
                    {
                        return func(&param);
                    }
                };

                // User string event: for string only logging
                struct user_string_event : public base_event<user_string_event>
                {
                    // Constructor
                    user_string_event() : base_event<user_string_event>()
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "user_string_event";
                    }

                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::user_string_event);
                };

                // ---------------------------------------------------------------------------
                // EVENT LIST. PLEASE KEEP IT ORDERED ALPHABETICALLY.
                // ---------------------------------------------------------------------------

                // A buffer acquire was initiated.
                struct buffer_acquire_initiated : public base_event<buffer_acquire_initiated>
                {
                    // Event name
                    static const char* get_name()
                    {
                        return "buffer_acquire_initiated";
                    }

                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::buffer_acquire_initiated);
                };

                // A buffer release was initiated.
                struct buffer_release_initiated : public base_event<buffer_release_initiated>
                {
                    // Event name
                    static const char* get_name()
                    {
                        return "buffer_release_initiated";
                    }

                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::buffer_release_initiated);
                };

                // Task's buffer set was acquired.
                struct buffer_set_acquired : public base_event<buffer_set_acquired>
                {
                    // Event name
                    static const char* get_name()
                    {
                        return "buffer_set_acquired";
                    }

                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::buffer_set_acquired);
                };

                // Somebody tries to cancel a group.
                struct group_canceled : public single_group_event<group_canceled>
                {
                    // Constructor
                    // @param g Pointer to group.
                    // @param success True if it was the first call to g->cancel().
                    group_canceled(group* g, bool success) : single_group_event<group_canceled>(g), _success(success)
                    {
                    }

                    // Checks whether this was the first time canceled() was called
                    // on the group.
                    // @return
                    // true -- This was the first time anyone requested the group to be canceled.
                    // false -- Somebody requested the group to be canceled before.
                    bool get_success() const
                    {
                        return _success;
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "group_canceled";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::group_canceled);

                private:
                    const bool _success;
                };

                // New group created
                struct group_created : public single_group_event<group_created>
                {
                    // Constructor
                    // @param g Pointer to group.
                    explicit group_created(group* g) : single_group_event<group_created>(g)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "group_created";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::group_created);
                };

                // Group destroyed (group destructor executes)
                struct group_destroyed : public single_group_event<group_destroyed>
                {
                    // Constructor
                    // @param g Pointer to group.
                    explicit group_destroyed(group* g) : single_group_event<group_destroyed>(g)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "group_destroyed";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::group_destroyed);
                };

                // Group ref count increases
                struct group_reffed : public group_ref_count_event<group_reffed>
                {
                    // Constructor
                    // @param g Group whose refcount increased
                    // @param count New refcount value
                    group_reffed(group* g, size_t count) : group_ref_count_event<group_reffed>(g, count)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "group_reffed";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::group_reffed);
                };

                // Group ref count decreases
                struct group_unreffed : public group_ref_count_event<group_unreffed>
                {
                    // Constructor
                    // @param g Group whose refcount increased
                    // @param count New refcount value
                    group_unreffed(group* g, size_t count) : group_ref_count_event<group_unreffed>(g, count)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "group_unreffed";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::group_unreffed);
                };

                // Group ref count decreases
                struct group_wait_for_ended : public single_group_event<group_wait_for_ended>
                {
                    // Constructor
                    // @param g Group whose refcount increased
                    explicit group_wait_for_ended(group* g) : single_group_event<group_wait_for_ended>(g)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "group_wait_for_ended";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::group_wait_for_ended);
                };

                // Join ut_cache
                struct join_ut_cache : public single_task_event<join_ut_cache>
                {
                    // Constructor
                    // @param t Pointer to task.

                    explicit join_ut_cache(task* g) : single_task_event<join_ut_cache>(g)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "join_ut_cache";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::join_ut_cache);
                };

                // Object refcount increased
                struct object_reffed : public object_ref_count_event<object_reffed>
                {
                    // Constructor
                    // @param o Object whose refcount increased
                    // @param count New refcount value
                    object_reffed(void* o, size_t count) : object_ref_count_event<object_reffed>(o, count)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "object_reffed";
                    }

                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::object_reffed);
                };

                // Object refcount decreased
                struct object_unreffed : public object_ref_count_event<object_unreffed>
                {
                    // Constructor
                    // @param g Group whose refcount decreased
                    // @param count New refcount value

                    object_unreffed(void* o, size_t count) : object_ref_count_event<object_unreffed>(o, count)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "object_unreffed";
                    }

                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::object_unreffed);
                };

                // User called hetcompute::runtime_shutdown.
                struct runtime_disabled : public base_event<runtime_disabled>
                {
                    // Event name
                    static const char* get_name()
                    {
                        return "runtime_disabled";
                    }

                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::runtime_disabled);
                };

                //  User calls hetcompute::runtime_init.
                struct runtime_enabled : public base_event<runtime_enabled>
                {
                    // Constructor
                    // @param num_exec_ctx Number of execution contexts.

                    explicit runtime_enabled(size_t num_exec_ctx) : _num_exec_ctx(num_exec_ctx)
                    {
                    }

                    // Returns number of execution contexts
                    size_t get_num_exec_ctx() const
                    {
                        return _num_exec_ctx;
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "runtime_enabled";
                    }

                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::runtime_enabled);

                private:
                    const size_t _num_exec_ctx;
                };

                // Scheduler added task to bundle for task subgraph acceleration
                struct scheduler_bundled_task : public single_task_event<scheduler_bundled_task>
                {
                    // Constructor
                    // @param t Pointer to task which was scheduled as a bundle.

                    explicit scheduler_bundled_task(task* t) : single_task_event<scheduler_bundled_task>(t)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "scheduler_bundled_task";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::scheduler_bundled_task);
                };

                // User creates dependency between two tasks
                struct task_after : public dual_task_event<task_after>
                {
                    // Constructor
                    // @param pred Pointer to predecessor task
                    // @param pred Pointer to successor task

                    task_after(task* pred, task* succ) : dual_task_event<task_after>(pred, succ)
                    {
                    }

                    // Returns predecessor task
                    task* get_pred() const
                    {
                        return get_task();
                    }

                    // Returns successor task
                    task* get_succ() const
                    {
                        return get_other_task();
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "task_after";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::task_after);
                };

                // Task starts cleanup sequence
                struct task_cleanup : public single_task_event<task_cleanup>
                {
                    // Constructor
                    // @param t Task pointer
                    // @param canceled true if task was canceled, false otherwise.
                    // @param in_utcache true if task was in ut_cache, false otherwise.

                    task_cleanup(task* t, bool canceled, bool in_utcache)
                        : single_task_event<task_cleanup>(t), _canceled(canceled), _in_utcache(in_utcache)
                    {
                    }

                    // Checks whether the task was canceled or not.
                    //
                    // @return
                    // true -- The task was canceled.
                    // false -- The task was not canceled.

                    bool get_canceled() const
                    {
                        return _canceled;
                    }

                    // Checks whether the task was in ut_cache or not.
                    //
                    // @return
                    // true -- The task was canceled.
                    // false -- The task was not canceled.

                    bool get_in_utcache() const
                    {
                        return _in_utcache;
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "task_cleanup";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::task_cleanup);

                private:
                    const bool _canceled;
                    const bool _in_utcache;
                };

                // OpenCL runtime invokes completion callback.
                struct task_gpu_completion_callback_invoked : public single_task_event<task_gpu_completion_callback_invoked>
                {
                    // Constructor
                    // @param t Pointer to task.

                    explicit task_gpu_completion_callback_invoked(task* t) : single_task_event<task_gpu_completion_callback_invoked>(t)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "task_gpu_completion_callback_invoked";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::task_gpu_completion_callback_invoked);
                };

                // Hetcompute creates new task.
                struct task_created : public single_task_event<task_created>
                {
                    // Constructor
                    // @param t Pointer to task.

                    explicit task_created(task* g) : single_task_event<task_created>(g)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "task_created";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::task_created);
                };

                // Task destructor called.
                struct task_destroyed : public single_task_event<task_destroyed>
                {
                    // Constructor
                    // @param t Pointer to task.

                    explicit task_destroyed(task* t) : single_task_event<task_destroyed>(t)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "task_destroyed";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::task_destroyed);
                };

                // Scheduler creates dynamic dependency between two tasks
                struct task_dynamic_dep : public dual_task_event<task_dynamic_dep>
                {
                    // Constructor
                    // @param pred Pointer to predecessor task
                    // @param pred Pointer to successor task

                    task_dynamic_dep(task* pred, task* succ) : dual_task_event<task_dynamic_dep>(pred, succ)
                    {
                    }

                    // Returns predecessor task
                    task* get_pred() const
                    {
                        return get_task();
                    }

                    // Returns successor task
                    task* get_succ() const
                    {
                        return get_other_task();
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "task_dynamic_dep";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::task_dynamic_dep);
                };

                // Task starts executing.
                struct task_executes : public single_task_event<task_executes>
                {
                    // Constructor
                    // @param t   Pointer to task.
                    // @param gpu Task is a gpu task
                    // @param blocking Task is a blocking task
                    // @param inlined Task is executed inline by main/foreign thread
                    // @param gettid Function that when invoked gets the device thread id on
                    //               which task executes

                    task_executes(task* t, bool gpu, bool blocking, bool inlined, std::function<device_thread_id(void)> gettid)
                        : single_task_event<task_executes>(t), _is_gpu(gpu), _is_blocking(blocking), _is_inline(inlined)
                    {
                        if (!gpu && !blocking && !inlined)
                        {
                            _tid = gettid();
                        }
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "task_executes";
                    }

                    // Id of thread on which task executes
                    device_thread_id get_tid()
                    {
                        return _tid;
                    }

                    // Returns whether task is a blocking task
                    bool is_blocking()
                    {
                        return _is_blocking;
                    }

                    // Returns whether task is a gpu task
                    bool is_gpu()
                    {
                        return _is_gpu;
                    }

                    // Returns whether task is executed inline by the main/foreign thread
                    bool is_inline()
                    {
                        return _is_inline;
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::task_executes);

                private:
                    bool             _is_gpu;      // Is this a gpu task?
                    bool             _is_blocking; // Is this a blocking task?
                    bool             _is_inline;   // Is this task executed inline by main/foreign thread?
                    device_thread_id _tid;         // id of thread on which it executes
                };

                // Task finished.
                struct task_finished : public single_task_event<task_finished>
                {
                    // Constructor
                    // @param t Pointer to task.

                    explicit task_finished(task* t) : single_task_event<task_finished>(t)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "task_finished";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::task_finished);
                };

#ifdef HETCOMPUTE_HAVE_GPU
                // Task launched into the GPU runtime
                struct task_launched_into_gpu : public single_task_event<task_launched_into_gpu>
                {
                    // Constructor
                    // @param t Pointer to task which was launched into the GPU runtime.

                    explicit task_launched_into_gpu(task* t) : single_task_event<task_launched_into_gpu>(t)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "task_launched_into_gpu";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::task_launched_into_gpu);
                };
#endif // HETCOMPUTE_HAVE_GPU

                // Task is sent to the runtime
                struct task_sent_to_runtime : public single_task_event<task_sent_to_runtime>
                {
                    // Constructor
                    // @param t Pointer to task.

                    explicit task_sent_to_runtime(task* t) : single_task_event<task_sent_to_runtime>(t)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "task_sent_to_runtime";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::task_sent_to_runtime);
                };

                // Task stolen.
                struct task_stolen : public single_task_event<task_stolen>
                {
                    // Constructor
                    // @param t    Pointer to task.
                    // @param from Id of queue from which task was stolen.

                    task_stolen(task* t, size_t from) : single_task_event<task_stolen>(t), _from(from)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "task_stolen";
                    }

                    // Id of queue from which task was stolen
                    size_t get_tq_from()
                    {
                        return _from;
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::task_stolen);

                private:
                    size_t _from; // id of queue from which task was stolen
                };

                // Task queue list is created
                struct task_queue_list_created : public base_event<task_queue_list_created>
                {
                    // @param num_qs   Number of queues
                    // @param foreignq Id of foreign queue
                    // @param mainq    Id of main queue

                    task_queue_list_created(size_t num_qs, size_t foreignq, size_t mainq, size_t deviceqs)
                        : base_event<task_queue_list_created>(), _num_qs(num_qs), _foreignq(foreignq), _mainq(mainq), _deviceqs(deviceqs)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "task_queue_list_created";
                    }

                    // Get number of queues
                    // @return Number of queues

                    size_t get_num_queues()
                    {
                        return _num_qs;
                    }

                    // Get foreign queue id
                    // @return Id of foreign queue

                    size_t get_foreign_queue()
                    {
                        return _foreignq;
                    }

                    // Get main queue id
                    // @return Id of main queue

                    size_t get_main_queue()
                    {
                        return _mainq;
                    }

                    // Get device queue id
                    // @return Id of first device queue

                    size_t get_device_queues()
                    {
                        return _deviceqs;
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::task_queue_list_created);

                private:
                    size_t _num_qs, _foreignq, _mainq, _deviceqs;
                };

                // Task ref count increases
                struct task_reffed : public task_ref_count_event<task_reffed>
                {
                    // @param t Task whose refcount increased
                    // @param count New refcount value

                    task_reffed(task* t, size_t count) : task_ref_count_event<task_reffed>(t, count)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "task_reffed";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::task_reffed);
                };

                // Task ref count decreases
                struct task_unreffed : public task_ref_count_event<task_unreffed>
                {
                    // @param t Task whose refcount decreased
                    // @param count New refcount value

                    task_unreffed(task* t, size_t count) : task_ref_count_event<task_unreffed>(t, count)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "task_unreffed";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::task_unreffed);
                };

                // Someone waits for task to finish
                struct task_wait : public single_task_event<task_wait>
                {
                    // Constructor
                    // @param t Pointer to task to wait for.
                    // @param wait_required true if we had to wait for the task. False
                    // otherwise.

                    task_wait(task* t, bool wait_required) : single_task_event<task_wait>(t), _wait_required(wait_required)
                    {
                    }

                    // Checks whether we had to wait for the task or not.

                    // @return
                    // true -- We had to wait for the task.
                    // false -- We didn't have to wait for the task.

                    bool get_wait_required() const
                    {
                        return _wait_required;
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "task_wait";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::task_wait);

                private:
                    const bool _wait_required;
                };

                // Task waited on was executed inline
                struct task_wait_inlined : public single_task_event<task_wait_inlined>
                {
                    // Constructor
                    // @param t Pointer to task on which to wait.

                    explicit task_wait_inlined(task* t) : single_task_event<task_wait_inlined>(t)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "task_wait_inlined";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::task_wait_inlined);
                };

                // trigger_task_scheduled
                struct trigger_task_scheduled : public single_task_event<trigger_task_scheduled>
                {
                    // Constructor
                    // @param t Pointer to task.

                    explicit trigger_task_scheduled(task* t) : single_task_event<trigger_task_scheduled>(t)
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "trigger_task_scheduled";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::trigger_task_scheduled);
                };

                struct ws_tree_new_slab : public base_event<ws_tree_new_slab>
                {
                    // Constructor
                    // @param n Pointer to ws_node_base

                    ws_tree_new_slab() : base_event<ws_tree_new_slab>()
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "ws_tree_new_slab";
                    }
                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::ws_tree_new_slab);
                };

                struct ws_tree_node_created : public base_event<ws_tree_node_created>
                {
                    // Constructor
                    // @param n Pointer to ws_node_base

                    ws_tree_node_created() : base_event<ws_tree_node_created>()
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "ws_tree_node_created";
                    }

                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::ws_tree_node_created);
                };

                struct ws_tree_worker_try_own : public base_event<ws_tree_worker_try_own>
                {
                    ws_tree_worker_try_own() : base_event<ws_tree_worker_try_own>()
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "ws_tree_worker_try_own";
                    }

                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::ws_tree_worker_try_own);
                };

                struct ws_tree_try_own_success : public base_event<ws_tree_try_own_success>
                {
                    ws_tree_try_own_success() : base_event<ws_tree_try_own_success>()
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "ws_tree_try_own_success";
                    }

                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::ws_tree_worker_try_own_success);
                };

                struct ws_tree_worker_try_steal : public base_event<ws_tree_worker_try_steal>
                {
                    ws_tree_worker_try_steal() : base_event<ws_tree_worker_try_steal>()
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "ws_tree_worker_try_steal";
                    }

                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::ws_tree_worker_try_steal);
                };

                struct ws_tree_try_steal_success : public base_event<ws_tree_try_steal_success>
                {
                    ws_tree_try_steal_success() : base_event<ws_tree_try_steal_success>()
                    {
                    }

                    // Event name
                    static const char* get_name()
                    {
                        return "ws_tree_try_steal_success";
                    }

                    // Event ID
                    static const event_id s_id = static_cast<event_id>(event_id_list::ws_tree_worker_try_steal_success);
                };

            }; // namespace events
        };     // namespace log
    };         // namespace internal
};             // namespace hetcompute
