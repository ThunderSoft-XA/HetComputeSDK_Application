/** @file bounded_lfqueue.hh*/
#pragma once

#include <hetcompute/internal/queues/internal_bounded_lfqueue.hh>
#include <hetcompute/internal/util/macros.hh>

namespace hetcompute
{
    /** @addtogroup blf_queue
    @{ */
    /**
      A Bounded Lock-Free FIFO Queue.

      @note1 The size of the queue is bounded at creation time.

    */

    template <typename T>
    class bounded_lfqueue
    {
    public:
        typedef typename internal::blfq::blfq_size_t<T, (sizeof(size_t) >= sizeof(T))> container_type;
        typedef T value_type;

        HETCOMPUTE_DELETE_METHOD(bounded_lfqueue(bounded_lfqueue const&));
        HETCOMPUTE_DELETE_METHOD(bounded_lfqueue(bounded_lfqueue&&));
        HETCOMPUTE_DELETE_METHOD(bounded_lfqueue& operator=(bounded_lfqueue const&));
        HETCOMPUTE_DELETE_METHOD(bounded_lfqueue& operator=(bounded_lfqueue&&));

        /** Constructs the Bounded Lock-Free Queue, given the log (base 2) of the maximum number of
         *  entries it can contain.
         *
         *  @param log_size Log (base 2) of the maximum number of entries in each node.
         */
        explicit bounded_lfqueue(size_t log_size) : _c(log_size)
        {
        }

        /** Push value into the queue.
         *
         *  @param v Value to be pushed into the queue.
         *  @return True if the push was successful; false if the queue was full.
         */
        bool push(value_type const& v)
        {
            return (_c.push(v, false) != 0 ? true : false);
        }

        /** Pop from the queue, placing the popped value in the result.
         *
         *  @param r The object to store the popped value in, if successful.
         *  @return True if the pop was successful; false if the queue was empty.
         *
         *  @note1 The contents of r are not modified if the pop is unsuccessful.
         */
        bool pop(value_type& r)
        {
            return (_c.pop(r, false) != 0 ? true : false);
        }

    private:
        container_type _c;
    };

};        // namespace hetcompute
/** @} */ /* end_addtogroup blf_queue */
