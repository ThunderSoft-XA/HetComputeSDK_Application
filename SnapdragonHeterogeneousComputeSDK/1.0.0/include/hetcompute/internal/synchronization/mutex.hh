#pragma once

#include <atomic>
#include <cstdlib>
#include <mutex>
#include <queue>
#include <thread>

#include <hetcompute/internal/synchronization/blocked_task.hh>
#include <hetcompute/internal/synchronization/mutex_cv.hh>
#include <hetcompute/internal/task/task.hh>

namespace hetcompute
{
    namespace internal
    {
        /** @addtogroup sync
        @{
        */
        // tts_spin_lock - test and test and set lock that spins on a global
        // atomic bool until it successfully flips the bool to true (for held)
        // Currently yields to the HetCompute scheduler after exceeding a threshold
        // of spin iterations

        class tts_spin_lock
        {
        private:
            std::atomic<bool> _held;

        public:
            tts_spin_lock() : _held(false)
            {
            }

            HETCOMPUTE_DELETE_METHOD(tts_spin_lock(tts_spin_lock&));
            HETCOMPUTE_DELETE_METHOD(tts_spin_lock(tts_spin_lock&&));
            HETCOMPUTE_DELETE_METHOD(tts_spin_lock& operator=(tts_spin_lock const&));
            HETCOMPUTE_DELETE_METHOD(tts_spin_lock& operator=(tts_spin_lock&&));

            void lock();
            bool try_lock();
            void unlock();
        };

        class futex_locked_queue
        {
        private:
            tts_spin_lock             _lock;
            std::queue<blocked_task*> _queue;

        public:
            futex_locked_queue() : _lock(), _queue()
            {
            }

            void enqueue(blocked_task* task)
            {
                _lock.lock();
                _queue.push(task);
                _lock.unlock();
            }

            blocked_task* dequeue()
            {
                blocked_task* result = nullptr;
                _lock.lock();
                // reuse unlcoked_deque
                result = unlocked_dequeue();
                _lock.unlock();
                return result;
            }

            blocked_task* unlocked_dequeue()
            {
                blocked_task* result = nullptr;
                if (_queue.empty() == false)
                {
                    result = _queue.front();
                    _queue.pop();
                }
                return result;
            }

            tts_spin_lock& get_lock()
            {
                return _lock;
            }
        };

        // Provides wait and wakeup facilities for tasks to
        // block within HetCompute (rather than in the OS) and be
        // explicitly woken up by another task
        /**
         *   Provides wait/wakeup capabilities for implementing
         *   synchronization primitives.
         *
         *   Allows a task to wait for another task to explicitly wake it
         *   up. Does not block to the operating system.
         */
        class futex
        {
        private:
            futex_locked_queue  _blocked_tasks;
            std::atomic<size_t> _waiters;
            std::atomic<size_t> _awake;

        public:
            /**
             * @brief Default constructor.
             *
             * Default constructor.
             */
            futex() : _blocked_tasks(), _waiters(0), _awake(0)
            {
            }

            HETCOMPUTE_DELETE_METHOD(futex(futex&));
            HETCOMPUTE_DELETE_METHOD(futex(futex&&));
            HETCOMPUTE_DELETE_METHOD(futex& operator=(futex const&));
            HETCOMPUTE_DELETE_METHOD(futex& operator=(futex&&));

            // Causes the task to block until another task calls wakeup
            //
            // If this is the first call to wait after a call to
            // wakeup failed to wakeup any tasks, the task will not block
            // This condition prevents races
            // between calls to wait and wakeup from not waking
            // any tasks and allows synchronization primitives
            // to be oblivious of whether or not any tasks are
            // currently waiting on the futex
            /**
             * Current task will block to the Qualcomm HetCompute scheduler until
             * another task calls wakeup.
             */
            void wait(std::atomic<int>* check_value_ptr = nullptr, int check_value = 0);

            std::atomic<int>* timed_wait(std::atomic<int>* check_value_ptr = nullptr, int check_value = 0);

            // Will attempt to wake up at least 1 waiting task
            //
            // If unable to wake up any waiting tasks (due to none
            // waiting or a race between wait and wakeup), wakeup
            // indicates that the next call to wait should not block
            // (i.e., the next task to call wait() is the task that is
            // woken up by the failed call to wakeup(...)
            /**
             * @brief Attempts to wake up waiting tasks.
             *
             *
             * Attempts to wake up <code>num_tasks</code> waiting tasks.
             * <code>wakeup</code> Will only wake up tasks if enough exist to be
             * woken up.
             *
             * If <code>num_tasks</code> is 0, <code>wakeup</code> will wake up
             * as many tasks as possible.
             *
             * @param num_tasks Number of tasks to attempt to wake up.
             * @return Number of tasks that were successfully woken up.
             */
            size_t wakeup(size_t num_tasks);

        private:
            // Attempts to wakeup a single task
            //
            // returns 1 if successful, 0 if not
            // param unlocked true Do unlocked access to queue
            //                false Use normal locked access to queue
            //                Used for broadcast, which locks, dequeues all tasks, and
            //                then unlocks, rather than repeatedly locking
            size_t wakeup_one(bool unlocked = false);
        };

        // Mutex that uses the futex for blocking when a
        // lock acquire fails
        /**
         * Provides basic mutual exclusion.
         *
         * Provides exclusive access to a single task or thread without
         * blocking the Qualcomm HetCompute scheduler.
         */
        class mutex
        {
        private:
            std::atomic<int> _state;
            futex            _futex;

        public:
            // Low threshold because yield() is used to backoff between spins
            static const int SPIN_THRESHOLD = 10;

            /**
             *  @brief Default constructor.
             *
             *  Default constructor. Initializes to mutex not locked.
             */
            mutex() : _state(0), _futex()
            {
            }

            HETCOMPUTE_DELETE_METHOD(mutex(mutex&));
            HETCOMPUTE_DELETE_METHOD(mutex(mutex&&));
            HETCOMPUTE_DELETE_METHOD(mutex& operator=(mutex const&));
            HETCOMPUTE_DELETE_METHOD(mutex& operator=(mutex&&));

            /**
             * @brief Acquires the mutex.
             *
             * Acquires mutex and waits if the mutex is not available.
             * May yield to the Qualcomm HetCompute scheduler so that other tasks
             * can execute.
             */
            void lock();

            /**
             * @brief Attempts to acquire the mutex.
             *
             * Attempts to acquire the mutex. Returns immediately
             * if the mutex is busy.
             *
             * @return
             * <code>true</code> -- Successfully acquired the mutex.\n
             * <code>false</code> -- Did not acquire the mutex.
             */
            bool try_lock();

            /**
             * @brief Releases the mutex.
             *
             * Releases the mutex.
             */
            void unlock();
        };

        /// Adds an id and number of permits (for that id) to
        /// allow a task/thread to call lock and unlock multiple times
        template <class Lock>
        class recursive_lock
        {
        private:
            Lock            _lock;
            std::thread::id _current_holder;
            size_t          _permits;

            std::thread::id get_id()
            {
                return std::this_thread::get_id();
            }

            bool is_owner()
            {
                std::thread::id task_id = get_id();
                return _current_holder == task_id;
            }

            void set_owner()
            {
                _current_holder = get_id();
                _permits        = 1;
            }

        public:
            recursive_lock() : _lock(), _current_holder(), _permits(0)
            {
            }

            HETCOMPUTE_DELETE_METHOD(recursive_lock(recursive_lock&));
            HETCOMPUTE_DELETE_METHOD(recursive_lock(recursive_lock&&));
            HETCOMPUTE_DELETE_METHOD(recursive_lock& operator=(recursive_lock const&));
            HETCOMPUTE_DELETE_METHOD(recursive_lock& operator=(recursive_lock&&));

            void lock()
            {
                if (is_owner())
                {
                    ++_permits;
                }
                else
                {
                    _lock.lock();
                    set_owner();
                }
            }

            bool try_lock()
            {
                if (is_owner())
                {
                    ++_permits;
                    return true;
                }
                else
                {
                    bool success = _lock.try_lock();
                    if (success)
                    {
                        set_owner();
                    }
                    return success;
                }
            }

            void unlock()
            {
                HETCOMPUTE_INTERNAL_ASSERT(is_owner(), "Non-owner thread is trying to unlock a recursive_lock.");
                HETCOMPUTE_INTERNAL_ASSERT(_permits > 0, "Trying to unlock a recursive_lock before locking.");
                --_permits;
                if (_permits == 0)
                {
                    _current_holder = std::thread::id();
                    _lock.unlock();
                }
            }
        };

        /** @} */ /* end_addtogroup sync */

    }; // namespace internal

}; // namespace hetcompute
