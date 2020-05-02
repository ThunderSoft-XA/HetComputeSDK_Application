#pragma once

#include <atomic>
#include <cstdlib>

#include <hetcompute/internal/legacy/task.hh>
#include <hetcompute/internal/synchronization/mutex.hh>

// Contains barrier interface for HetCompute applications
//
// Currently implemented as a sense reversing spin-barrier
// _count is atomically decremented by each task that waits on the
// barrier.  When _count reaches 0, the task that last decremented
// _count negates _sense to allow the waiting tasks to continue
// execution.
//
// When a task exceeds the SPIN_THRESHOLD, all further waiting
// tasks will yield() to the HetCompute scheduler
//
// For responsiveness, some amount of spinning is done before
// yielding to the HetCompute scheduler

namespace hetcompute
{
    namespace internal
    {
        class sense_barrier
        {
        private:
            // Counts the number of tasks that still need to reach the barrier
            std::atomic<size_t> _count;
            // Total number of tasks that will wait on the barrier
            // Does not change after initialization
            size_t _total;
            // Current direction of the barrier
            std::atomic<bool> _sense;
            // Tasks (for each sense) to wait on if too much spinning occurs
            task_shared_ptr _wait_task[2];

            size_t _delta;

        public:
            static const int WAIT_TASK_SPIN_THRESHOLD = 1000;
            static const int SPIN_THRESHOLD           = 10000;

            void wait();

            explicit sense_barrier(size_t count) : _count(count), _total(count), _sense(false), _wait_task(), _delta(_total / (4))
            {
                _wait_task[0] = nullptr;
                _wait_task[1] = nullptr;
            }

            HETCOMPUTE_DELETE_METHOD(sense_barrier(sense_barrier&));
            HETCOMPUTE_DELETE_METHOD(sense_barrier(sense_barrier&&));
            HETCOMPUTE_DELETE_METHOD(sense_barrier& operator=(sense_barrier const&));
            HETCOMPUTE_DELETE_METHOD(sense_barrier& operator=(sense_barrier&&));

        private:
            bool volatile_read_sense() { return _sense.load(hetcompute::mem_order_relaxed); }

            void create_wait_task(bool local_sense);
        };

    }; // namespace internal

    typedef internal::sense_barrier barrier;

}; // namespace hetcompute
