#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include <hetcompute/internal/synchronization/cvstatus.hh>
#include <hetcompute/internal/synchronization/mutex.hh>

namespace hetcompute
{
    namespace internal
    {
        // Condition variable for HetCompute applications
        //
        // Currently implemented using a futex, which
        // contains wait and wakeup methods that mirror
        // the semantics of the condition variable

        // condition_variable implements condition variable using a futex and the specified Lock class
        /** @addtogroup sync
        @{ */
        /**
            Provides inter-task communication via wait() and notify().

            Provides inter-task communication via wait() and notify() calls
            without blocking the HetCompute scheduler.
        */
        template <typename Lock>
        class condition_variable
        {
        private:
            futex _futex;

        public:
            static const size_t SPIN_THRESHOLD = 1000;

            condition_variable() : _futex()
            {
            }

            ~condition_variable()
            {
            }

            HETCOMPUTE_DELETE_METHOD(condition_variable(condition_variable&));
            HETCOMPUTE_DELETE_METHOD(condition_variable(condition_variable&&));
            HETCOMPUTE_DELETE_METHOD(condition_variable& operator=(condition_variable const&));
            HETCOMPUTE_DELETE_METHOD(condition_variable& operator=(condition_variable&&));

            void wakeup()
            {
                notify_all();
            }

            /**  Wakes up one waiting task. */
            void notify_one()
            {
                _futex.wakeup(1);
            }

            /**  Wakes up all waiting tasks. */
            void notify_all()
            {
                _futex.wakeup(0);
            }

            /** Task waits until woken up by another task.

                Task may yield to the HetCompute scheduler.

                @param lock Mutex associated with the condition_variable.
                Must be locked prior to calling wait. Mutex will be
                held by the task when wait returns.
            */
            void wait(Lock& lock)
            {
                lock.unlock();

                _futex.wait();

                lock.lock();
            }

            /** While the predicate is false, the task waits.

                Task may yield to the HetCompute scheduler.

                @param lock Mutex associated with the condition_variable.
                Must be locked prior to calling wait. lock will be
                held by the task when wait returns.

                @param pred The condition_variable will continue to wait
                until this Predicate is true.
            */
            template <class Predicate>
            void wait(Lock& lock, Predicate pred)
            {
                while (!pred())
                {
                    wait(lock);
                }
            }

            /** Task waits until either 1) woken by another task or
                  2) wait time has been exceeded.

                  Task may yield to the HetCompute scheduler.

                  @param lock Mutex associated with the condition_variable.
                  Mutex must be locked prior to calling wait_for.

                  @param rel_time Amount of time to wait to be woken up.

                  @return
                  std::cv_status::no_timeout - Task was woken up by another task
                  before rel_time was exceeded.  lock will be held by the task when
                  wait_for returns.
                  @par
                  std:cv_status::timeout - rel_time was exceeded before the
                  task was woken up.
              */
            template <class Rep, class Period>
            hetcompute::cv_status wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& rel_time)
            {
                auto end_time = std::chrono::high_resolution_clock::now() + rel_time;
                return wait_until(lock, end_time);
            }

            /** While the predicate is false, task waits until 1) woken by
                another task or 2) wait time has been exceeded.

                Task may yield to the HetCompute scheduler.

                @param lock Mutex associated with the condition_variable.
                Mutex must be locked prior to calling wait_for.

                @param rel_time Amount of time to wait to be woken up.

                @param pred The condition_variable will continue to wait
                until this Predicate is true.

                @return
                TRUE -- Task was woken up and predicate was true before rel_time
                was exceeded. lock will be held by the task when wait_for returns.
                @par
                FALSE -- Predicate was not true before rel_time was exceeded.
            */
            template <class Rep, class Period, class Predicate>
            bool wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& rel_time, Predicate pred)
            {
                while (!pred())
                {
                    if (wait_for(lock, rel_time) == std::cv_status::timeout)
                    {
                        return pred();
                    }
                }
                return true;
            }

            /** Task waits until either 1) woken by another task or
                2) wait time has been exceeded.

                Task may yield to the HetCompute scheduler.

                @param lock Mutex associated with the condition_variable.
                Mutex must be locked prior to calling wait_until.

                @param timeout_time Time to wait until.

                @return
                std::cv_status::no_timeout -- Task was woken up by another task
                before timeout_time was exceeded. lock will be held by the task when
                wait_until returns.
                @par
                std:cv_status::timeout -- timeout_time was exceeded before the
                task was woken up.
            */
            template <class Clock, class Duration>
            hetcompute::cv_status wait_until(Lock& lock, const std::chrono::time_point<Clock, Duration>& timeout_time)
            {
                lock.unlock();

                std::atomic<int>* state = _futex.timed_wait();

                while (std::chrono::high_resolution_clock::now() < timeout_time)
                {
                    if (*state == 1)
                    {
                        break;
                    }
                    yield();
                }

                lock.lock();

                // Signal that we're done waiting
                int prev_state = state->exchange(1);
                if (prev_state == 0)
                {
                    // Timeout occurred before blocked_task in the futex was "woken"
                    return std::cv_status::timeout;
                }

                // Successfully woken up before timeout.  We have the last
                // reference to _state because the blocked_task has already been
                // woken up
                delete state;
                return std::cv_status::no_timeout;
            }

            /** While the predicate is false, task waits until 1) woken by
                another task or 2) wait time has been exceeded.

                Task may yield to the HetCompute scheduler.

                @param lock Mutex associated with the condition_variable.
                Mutex must be locked prior to calling wait_until.

                @param timeout_time Amount of time to wait to be woken up.

                @param pred The condition_variable will continue to wait
                until this Predicate is true.

                @return
                TRUE -- Task was woken up and predicate was true before timeout_time
                was exceeded. lock will be held by the task when wait_until returns.
                @par
                FALSE -- Predicate was not true before timeout_time was exceeded.
            */
            template <class Clock, class Duration, class Predicate>
            bool wait_until(Lock& lock, const std::chrono::time_point<Clock, Duration>& timeout_time, Predicate pred)
            {
                while (!pred())
                {
                    if (wait_until(lock, timeout_time) == std::cv_status::timeout)
                    {
                        return pred();
                    }
                }
                return true;
            }
        };

        // condvar_mutex is defined to use condition_variable class with hetcompute::internal::mutex as the lock.
        typedef condition_variable<std::unique_lock<hetcompute::internal::mutex>> condvar_mutex;

        /** @} */ /* end_addtogroup sync */

    }; // namespace hetcompute::internal

}; // namespace hetcompute
