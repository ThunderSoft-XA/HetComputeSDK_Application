/** @file lfqueue.hh*/
#pragma once

#include <hetcompute/internal/queues/internal_lfqueue.hh>
#include <hetcompute/internal/util/macros.hh>

namespace hetcompute
{
    /** @addtogroup lf_queue
    @{ */
    /**
      Unbounded Lock-Free FIFO queue that is capable of dynamically growing and shrinking.
    */

    template <typename T>
    class lfqueue
    {
    public:
        typedef typename internal::lfq::lfq<T> container_type;
        typedef T                              value_type;

        HETCOMPUTE_DELETE_METHOD(lfqueue(lfqueue const&));
        HETCOMPUTE_DELETE_METHOD(lfqueue(lfqueue&&));
        HETCOMPUTE_DELETE_METHOD(lfqueue& operator=(lfqueue const&));
        HETCOMPUTE_DELETE_METHOD(lfqueue& operator=(lfqueue&&));

        /** Constructs the Unbounded Lock-Free Queue, given the log (base 2) of the size of the static array
         *  within each node.
         *
         *  @param log_size Log (base 2) of the maximum number of entries in each node.
         */
        explicit lfqueue(size_t log_size) : _c(log_size)
        {
        }

        /** Push value into the queue. Since the queue is capable of growing, a push always succeeds.
         *
         *  @param v Value to be pushed into the queue.
         *  @return True
         */
        bool push(value_type const& v)
        {
            return (_c.push(v) != 0 ? true : false);
        }

        /** Pop from the queue, placing the popped value in the result.
         *
         *  @param r The object to store the popped value in, if successful.
         *  @return True if the pop was successful; FALSE if the queue was empty.
         *
         *  @note1 the contents of r are not modified if the pop is unsuccessful.
         */
        bool pop(value_type& r)
        {
            return (_c.pop(r) != 0 ? true : false);
        }

    private:
        container_type _c;
    };

};        // namespace hetcompute
/** @} */ /* end_addtogroup lf_queue */
