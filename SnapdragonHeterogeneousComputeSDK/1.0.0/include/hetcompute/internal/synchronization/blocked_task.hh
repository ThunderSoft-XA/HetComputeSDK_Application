#pragma once

#include <memory>

#include <hetcompute/internal/legacy/types.hh>
#include <hetcompute/internal/legacy/task.hh>
#include <hetcompute/internal/synchronization/semaphore.hh>
#include <hetcompute/internal/task/task_shared_ptr.hh>

namespace hetcompute
{
    namespace internal
    {
        class scheduler;
        class exectx;

        /*
            Contains a hierarchy of types of blocked_task
            that may be used with the futex.

            wakeup() is the same for blocked_hetcompute_task and
            blocked_blocking_task (from non-HetCompute threads)
            because both use stub_tasks.

            blocked_timed_task overrides wakeup() because
            it doesn't actually block the calling thread
            to allow a timeout to be realized in a timely manner.
        */
        class blocked_task
        {
        protected:
            task_shared_ptr _stub_task;

        public:
            blocked_task() : _stub_task(nullptr)
            {
            }

            explicit blocked_task(task_shared_ptr stub_task) : _stub_task(stub_task)
            {
            }

            HETCOMPUTE_DELETE_METHOD(blocked_task(blocked_task&));
            HETCOMPUTE_DELETE_METHOD(blocked_task(blocked_task&&));
            HETCOMPUTE_DELETE_METHOD(blocked_task& operator=(blocked_task&));
            HETCOMPUTE_DELETE_METHOD(blocked_task& operator=(blocked_task&&));

            virtual ~blocked_task()
            {
            }

            virtual void wakeup()
            {
                // Launches the task that will wake up the blocked task
                // May not wake up on the same thread
                legacy::launch(_stub_task);
            }
        };

        // blocked_hetcompute_task is used for HetCompute threads that
        // call wait() on a futex
        class blocked_hetcompute_task : public blocked_task
        {
        private:
            scheduler* _sched;
            scheduler* _this_sched;

        public:
            blocked_hetcompute_task(scheduler& sched, task_shared_ptr stub_task, scheduler& this_sched)
                : blocked_task(stub_task), _sched(&sched), _this_sched(&this_sched)
            {
            }

            HETCOMPUTE_DELETE_METHOD(blocked_hetcompute_task(blocked_hetcompute_task&));
            HETCOMPUTE_DELETE_METHOD(blocked_hetcompute_task(blocked_hetcompute_task&&));
            HETCOMPUTE_DELETE_METHOD(blocked_hetcompute_task& operator=(blocked_hetcompute_task&));
            HETCOMPUTE_DELETE_METHOD(blocked_hetcompute_task& operator=(blocked_hetcompute_task&&));

            virtual ~blocked_hetcompute_task()
            {
            }

            // Starts up the new scheduler on the current device thread
            void block(exectx& cur_ctx);

            // Cleans up the scheduler that the task was woken up on
            void cleanup();
        };

        // blocked_blocking_task is used for non-HetCompute threads
        // that call wait() on a futex
        //
        // We cannot move these tasks to another thread, so we
        // simply use a semaphore to put the thread to sleep
        // and eventually wake it up using a stub task
        class blocked_blocking_task : public blocked_task
        {
        private:
            std::shared_ptr<semaphore> _sem;

        public:
            blocked_blocking_task() : _sem(std::make_shared<semaphore>())
            {
                _stub_task = create_stub_task([this] { _sem->signal(); });
            }

            blocked_blocking_task(task_shared_ptr stub_task, std::shared_ptr<semaphore> sem) : blocked_task(stub_task), _sem(sem)
            {
            }

            HETCOMPUTE_DELETE_METHOD(blocked_blocking_task(blocked_blocking_task&));
            HETCOMPUTE_DELETE_METHOD(blocked_blocking_task(blocked_blocking_task&&));
            HETCOMPUTE_DELETE_METHOD(blocked_blocking_task& operator=(blocked_blocking_task&));
            HETCOMPUTE_DELETE_METHOD(blocked_blocking_task& operator=(blocked_blocking_task&&));

            virtual ~blocked_blocking_task()
            {
            }

            // waits on the semaphore
            void block();
        };

        // blocked_timed_task is used for timed waits on
        // condition variables
        //
        // For wait_for and wait_until, we want to signal
        // that a timeout has occurred in a reasonable
        // amount of time, which is less possible if we
        // block the calling thread
        //
        // Instead, we insert a blocked_timed_task into the
        // futex queue and spin until either:
        // 1) the futex wakes up the blocked_timed_task OR
        // 2) the timeout occurs
        class blocked_timed_task : public blocked_task
        {
        private:
            std::atomic<int>* _state;

        public:
            blocked_timed_task() : _state(new std::atomic<int>())
            {
                *_state = 0;
            }

            HETCOMPUTE_DELETE_METHOD(blocked_timed_task(blocked_timed_task&));
            HETCOMPUTE_DELETE_METHOD(blocked_timed_task(blocked_timed_task&&));
            HETCOMPUTE_DELETE_METHOD(blocked_timed_task& operator=(blocked_timed_task&));
            HETCOMPUTE_DELETE_METHOD(blocked_timed_task& operator=(blocked_timed_task&&));

            virtual ~blocked_timed_task()
            {
            }

            // Sets _state to woken up and checks whether or
            // not it needs to be deleted
            virtual void wakeup();

            std::atomic<int>* get_state()
            {
                return _state;
            }
        };
    }; // namespace hetcompute::internal
};     // namespace hetcompute
