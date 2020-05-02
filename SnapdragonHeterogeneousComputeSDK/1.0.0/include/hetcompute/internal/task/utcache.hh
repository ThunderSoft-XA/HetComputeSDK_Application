#pragma once

#include <string>

namespace hetcompute
{
    namespace internal
    {
        class group;
        class task;

        /// This cache stores tasks that have been added to one or more groups
        /// but haven't been launched yet.
        ///
        /// We need this cache because groups do not keep a list of the tasks
        /// in them. Without it, if a task hasn't been launched belongs to a
        /// group that gets canceled, we have no way to notify the task that
        /// it has been canceled.
        namespace unlaunched_task_cache
        {
            /// Calculates size of the unlaunched task cache.
            ///
            /// @return
            /// Number of distinct tasks in unlaunched task cache.
            size_t get_size();

            /// Inserts task t in cache if group g is not canceled.
            ///
            /// @param t Task to insert.
            /// @param g Target group.
            ///
            /// @return
            /// true - The task has been added to the cache. \n
            /// false - The tash has not been added to the cache because g is canceled.
            bool insert(task* t, group* g);

            /// Removes task from cache.
            ///
            /// @param t Task to remove.
            void remove(task* t);

            /// Removes and cancels any task from cache that belongs to group g.
            ///
            /// @param g Canceled grup.
            void cancel_tasks_from_group(group* g);

            /// Returns a string that describes the state of the cache. Use this
            /// for debugging purposes only.
            ///
            /// @return String that describes the state of the cache.
            std::string to_string();

            /// Initializes the utcache
            ///
            ///
            void init_utcache();

            /// Shutdown the utcache
            ///
            ///
            void shutdown_utcache();

        };  // namespace hetcompute::internal::unlaunched_task_cache
    };  // namespace hetcompute::internal
};  // namespace hetcompute
