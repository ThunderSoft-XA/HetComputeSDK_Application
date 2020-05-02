#pragma once

#include <atomic>

#include <hetcompute/internal/atomic/atomicops.hh>
#include <hetcompute/internal/compat/compiler_compat.h>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/macros.hh>
#include <hetcompute/internal/util/memorder.hh>

#if HETCOMPUTE_BLFQ_FORCE_SEQ_CST
#define HETCOMPUTE_BLFQ_MO(x) hetcompute::mem_order_seq_cst
#else
#define HETCOMPUTE_BLFQ_MO(x) x
#endif

namespace hetcompute
{
    namespace internal
    {
        namespace blfq
        {
            // Implementation of a Bounded lock-free queue ( henceforth referred to as blfq or bounded_lfqueue) that provides FIFO semantics
            //
            // Notes about the Algorithm:
            // 1. Steal 1 bit from the first word of the array element to indicate if it is empty or not.
            // 2. For 32 bit architectures, the fetch_and_add of the tail is replaced with a CAS to prevent integer overflow.

            // The aim of this data structure is two-fold. First, it is a FIFO Multi-Producer Multi-Consumer Queue,
            // that can be used as the foreign task queue.
            // Second, this is a concurrent data structure that will be exposed to the users, which they can operate on with HETCOMPUTE.
            // Therefore, the queue must be able to handle any type of data that the user wants.
            // As the task queue, we only store task pointers (task*) in it, but it should be able to handle types
            // whose size is > sizeof(size_t). The template specialization is also given below.

            // The bounded_lfqueue is implemented as a cyclic array. Each array element consists of two contiguous words.
            // The array itself is static in size, with the ability to grow and shrink being provided by linking
            // multiple bounded lock-free queues in a linked list.
            //  Each array element contains the following 4 segments:
            //  safe     - 1 bit. The need for this bit is described in the bounded_lfqueue class
            //  empty    - 1 bit. 0 indicates non empty array element, and 1 indicates empty.
            //  idx      - remaining bits of word 1 (e.g. 62 bits on a 64 bit machine and 30 bits on a 32 bit machine)
            //  val      - 1 word (e.g. 64 bits on x86_64)
            // Additional Details:
            // 1.By including the empty bit, we now restrict idx to 2^62 for 64 bit systems and 2^30 for 32 bit systems.

            // element in the array.
            struct element
            {
#if HETCOMPUTE_HAS_ATOMIC_DMWORD
                typedef hetcompute_dmword_t element_t;
                element_t                 _safe : 1;  // indicate if element is open for push
                element_t                 _empty : 1; // indicate if element contains any data
#if HETCOMPUTE_SIZEOF_MWORD == 8
                element_t _idx : 62; // version counter to avoid ABA
                element_t _val : 64; // space for pointer to store in queue element
#elif HETCOMPUTE_SIZEOF_MWORD == 4
                element_t _idx : 30; // version counter to avoid ABA
                element_t _val : 32; // space for pointer to store in queue element
#else // HETCOMPUTE_SIZEOF_MWORD
#error "unknown HETCOMPUTE_SIZEOF_MWORD"
#endif // HETCOMPUTE_SIZEOF_MWORD
#else  // HETCOMPUTE_HAS_ATOMIC_DMWORD
#error "The Bounded Lock-Free Queue requires Double Machine Word atomics"
#endif // HETCOMPUTE_HAS_ATOMIC_DMWORD

                element(element_t safe, element_t empty, element_t idx, element_t val) : _safe(safe), _empty(empty), _idx(idx), _val(val) {}
                element() HETCOMPUTE_NOEXCEPT : _safe(), _empty(), _idx(), _val() {}
            };

            // The bounded_lfqueue class contains the cyclic (static) array of elements,
            // in addition to maintaining the indices of the head and tail of the queue.
            // Objects are enqueued into the array at the tail, and are dequeued from the head.
            // Notes about the algorithm:
            // 1. Enqueuers and dequeuers first perform a fetch and increment on the tail and head respectively.
            // The result of this fetch and increment is the id that the operation is going to be using.
            // The id is then used to index into the array and obtain the element that the operation is going to
            // be working on. At a high level, if the array is of size R, the enqueuer enqueues val in
            // index (id mod R) by changing the contents of the element from <safe,1,id,0> to <1,0,id,val>.
            // Here 'safe' indicates that the safebit can be 0 or 1. Similarly, a dequeuer with the same id
            // dequeues val by changing its contents from <1,1,id, val> to <1,0,id+R,0>.

            // For the rest of the description, an enqueuer and dequeuer with id 'x' are referred to as enq_x
            // and deq_x, respectively.

            // enq_x and deq_x are 'mated' in the sense that if enq_x succeeds in enqueueing its value, then
            // the value can only be dequeued by deq_x. Below is description of the algorithm, and includes
            // the scenario where deq_x arrives and finds that enq_x has not enqueued its val.

            // Since this is a concurrent algorithm, however, the following scenarios could arise.
            // At the dequeuer side:
            // Case 1: deq_x reads the element and finds it to be empty. Here, deq_x changes the contents of the
            // element to <safe,1,x+R,0> using a CAS instruction. This prevents enq_x from storing its value there.
            // Case 2: deq_x reads the element and finds it to be non-empty. deq_x then checks the id of the element.
            //   Case 2a: element's id == x. Here, deq_x dequeues the value using a cas instruction.
            //   Case 2b: element's id < x. In this case, the deq_id is in flight, but has not yet dequeued.
            //   So deq_x informs enq_x not to enqueue the value (as deq_x will not be there to dequeue it)
            //   by making the element unsafe. To achieve this, deq_x uses a cas instruction to change the safe bit
            //   to 0, without changing the contents of the other fields of the element.
            //   Case 2c: element's id > x. Here deq_x does not make any changes to the element and simply terminates.

            // At the enqueuer side: An enqueuer enqueues its value in an element only if the element is found to be empty.
            // If the element contains a value that has not been dequeued, the enqueuer checks if the node is full
            // by doing a (tail -head). If the node is full, then it closes the node.

            // Additional notes about the implementation:
            // 1. In the cyclic array, head and tail are only ever incremented. Therefore, the problem of
            // integer overflow arises when dealing with 32 bit systems. So we handle the 32 bit and 64 bit
            // cases slightly differently. In a 32 bit system, the incrementing is done using a CAS instruction.
            // If the increment would result in an overflow, the node is closed. In a 64 bit system, this is not
            // a practical issue as it would take many years for the limit to be reached.
            // 2. If a node in the ring contains an id idx and some valid data X, i.e. it is not empty, then X
            // can only be dequeued by dequeuer idx and not anyone else.
            // 3. The safe bit allows a dequeuer with id = idx to inform the corresponding enqueuer with id = idx
            // to not enqueue in that element.
            // 4. tail is a composite word consisting of a closed bit and an index. The closed bit (MSB) when
            // set to 1 indicates that the node is closed from further enqueues, i.e. it is full. The remaining
            // LSBs are used for the index. The index is restricted to 30 bits on a 32 bit system and 62 bits
            // on a 64 bit system as it must fit in the _idx field of an element. Recall that the 2 MSBs are stolen
            // from the first word of an element for the safe and empty bits.
            // 5. We ensure that the array is the size of a power of 2 (i.e. 2^k) to ensure that we can easily
            // determine the element to which an index maps by 'AND'ing with (2^k -1).

            class bounded_lfqueue
            {
            private:
                // A pusher may starve if it is continously overtaken by a stream of poppers that perform an empty
                // transition of the element. Note that this is still legal with respect to the progress guarantees
                // of the algorithm. In a lock-free (or non-blocking) algorithm, the only progress guarantee provided
                // is that some operation completes in a finite number of its own steps. In this case, the pop operations
                // terminate and the algorithm is, therefore, lock-free. However, in a case where there is a single
                // pusher and multiple poppers, it makes sense for the poppers to back off a bit to give the pusher
                // a chance, before performing the empty transition. The pushers keeps track of the number of times
                // it retries, and closes the node if it reaches this threshold.
                static constexpr size_t s_starving_limit = 10000;

                // Close the node.
                inline void close_node()
                {
                    // Set the most significant bit of _tail to 1 using an atomic fetch-or to indicate node closure.
#if HETCOMPUTE_SIZEOF_MWORD == 8
                    _tail.fetch_or(size_t(1) << 63, HETCOMPUTE_BLFQ_MO(hetcompute::mem_order_relaxed));
#elif HETCOMPUTE_SIZEOF_MWORD == 4
                    _tail.fetch_or(size_t(1) << 31, HETCOMPUTE_BLFQ_MO(hetcompute::mem_order_relaxed));
#else // HETCOMPUTE_SIZEOF_MWORD
#error "unknown HETCOMPUTE_SIZEOF_MWORD"
#endif
                }

                inline size_t extract_index_from_tail(size_t tail)
                {
                    // the MSB is the closed bit. So drop it by shifting to the left once and then back to the right.
                    return ((tail << 1) >> 1);
                }

                inline bool is_node_closed(size_t tail)
                {
// Check if the node is closed by determining if the MSB of tail is set
#if HETCOMPUTE_SIZEOF_MWORD == 8
                    return ((tail & (size_t(1) << 63)) != 0);
#elif HETCOMPUTE_SIZEOF_MWORD == 4
                    return ((tail & (size_t(1) << 31)) != 0);
#else // HETCOMPUTE_SIZEOF_MWORD
#error "unknown HETCOMPUTE_SIZEOF_MWORD"
#endif
                }

                // Called in the take() method when queue is empty and tail is less than head.
                // Note that head is the index from which an object is popped/stolen, and tail is the index where
                // objects are pushed. This resets tail to head when that happens so that an enqueuer does not get
                // an index for which the dequeuer has already finished.
                // Doing this prevents an enqueuer from having to increment _tail every time and then finding out
                // that the corresponding dequeuer has already completed. A dequeuer, on finding the queue to be
                // empty, bumps tail to head.
                // Additionally, in the case of the bounded lfqueue, if the node is closed, it is opened.
                // This is because the following scenario can happen:
                // 1. Consider a pusher who finds the queue to be full at some point in time. It then proceeds to close the node.
                // 2. Before it executes the fetch_or to close the node, poppers empty the queue.
                // 3. The pusher then closes the queue. Now we have an empty queue, which is closed. No pusher can enqueue as it
                //    performs a check that the queue is not closed before proceeding.
                // 4. Therefore, only a popper that finds the queue to be empty and attempts to fix the state, will reopen the node.
                void fix_state(bool is_unbounded_queue);

                // Open a closed node in the bounded lfqueue.
                void open_node();

                // Check that the increment does not overwrite empty and safe bits.
                inline bool ok_to_increment(size_t current)
                {
#if HETCOMPUTE_SIZEOF_MWORD == 8
                    if ((current + 1) < (size_t(1) << 62))
                    {
                        return true;
                    }
#elif HETCOMPUTE_SIZEOF_MWORD == 4
                    if ((current + 1) < (size_t(1) << 30))
                    {
                        return true;
                    }
#else // HETCOMPUTE_SIZEOF_MWORD
#error "unknown HETCOMPUTE_SIZEOF_MWORD"
#endif
                    return false;
                }

            public:
                explicit bounded_lfqueue(size_t logsz)
                    : _head(0),
                      _tail(0),
                      _logsz(logsz),
                      _max_array_size(size_t(1) << logsz), // _max_array_size is 2^logsz.
                      _array(nullptr)
                {
// Since 2 bits (_safe and _empty) are stolen from the first word of an element, the max value of
// _idx needs to be restricted. Hence, we assert that the maximum size of the array is less than 2^30
// for 32 bit systems and 2^62 for 64 bit systems.
#if HETCOMPUTE_SIZEOF_MWORD == 8
                    HETCOMPUTE_INTERNAL_ASSERT(logsz <= 62, "On 64 bit systems the largest size of the blfq array allowed is 2^62");
#elif HETCOMPUTE_SIZEOF_MWORD == 4
                    HETCOMPUTE_INTERNAL_ASSERT(logsz <= 30, "On 32 bit systems the largest size of the blfq array allowed is 2^30");
#else // HETCOMPUTE_SIZEOF_MWORD
#error "unknown HETCOMPUTE_SIZEOF_MWORD"
#endif

                    // Allocate the static array.
                    _array = new std::atomic<element>[ _max_array_size ];

                    // Initialize all entries in the array.
                    for (size_t i = 0; i < _max_array_size; i++)
                    {
                        element a;
                        a._safe  = 1;
                        a._empty = 1;
                        a._idx   = i;
                        a._val   = 0;

                        // atomic_strore is broken for aarch64-clang for 16B words, when compiling in release mode
                        // Clang generates the following assembly code:
                        // ldaxp   xzr, xzr, [x8]
                        // stlxp   w16, x26, xzr, [x8]
                        // According to the ARMv8-A architecture reference manual, p:4768
                        // For the LDAXP instruction -
                        // CONSTRAINED UNPREDICTABLE behavior:
                        // "  if t == t2, then one of the following behaviors can occur:
                        // * The instruction is undefined.
                        // * The instruction executes as a nop.
                        // * The instruction performs a load using the specified addressing mode, and the base register is set to an
                        //   UNKNOWN value
                        // @naravind: Here t = xzr, and t2= xzr. As a result, the
                        // In practice, an illegal instruction error was observed at runtime.
                        // The store to _array[i] here, however need not be atomic.
                        *(reinterpret_cast<element*>(&_array[i])) = a;
                    }
                    hetcompute_atomic_thread_fence(HETCOMPUTE_BLFQ_MO(hetcompute::mem_order_seq_cst));
                }

                // Destructor. Free the allocated array.
                ~bounded_lfqueue();

                // Delete the no argument constructor.
                HETCOMPUTE_DELETE_METHOD(bounded_lfqueue());

                // Delete the copy constructor, move constructor and the overloaded assignment operators
                // owing to the fact that the destructor deletes the array.
                HETCOMPUTE_DELETE_METHOD(bounded_lfqueue(bounded_lfqueue const&));
                HETCOMPUTE_DELETE_METHOD(bounded_lfqueue(bounded_lfqueue&&));
                HETCOMPUTE_DELETE_METHOD(bounded_lfqueue& operator=(bounded_lfqueue const&));
                HETCOMPUTE_DELETE_METHOD(bounded_lfqueue& operator=(bounded_lfqueue&&));

                // Close node, and then setup _head to point to the appropriate index.This is used in the bounded_buffer case
                // This is to prevent any further push operations from occuring at that node. Note that closing the node
                // is always successful.
                void lock_node();

                // Take method for BLFQ. Remove the element from the array at index head.
                // This function returns 0 if the queue is empty, and the difference between tail and head otherwise.
                // The reason for return of the unsafe size and not a boolean is that this information is
                // then used to determine if other device threads need to be signalled within the scheduler.
                size_t take(size_t * result, bool is_unbounded_queue);

                // Put method for Unbounded LFQ. Update the contents of the element at index tail to contain value.
                // Returns the unsafe size of the queue if successful, and 0 if the queue was full. Again, this
                // information is used in the scheduler to signal other device threads.
                size_t unbounded_put(size_t value);

                // Put method for Bounded LFQ. Update the contents of the element at index tail to contain value.
                // Returns the unsafe size of the queue if successful, and 0 if the queue was full. Again, this
                // information is used in the scheduler to signal other device threads.
                size_t bounded_put(size_t value);

                // Overwrite Put method for Bounded LFQ.
                // Returns:
                // 1. Max size of the queue if no entry had to be overwritten,
                // 2. Max size of the queue + 1, if an overwrite had to be performed,
                // 3. 0 if the queue node was closed.
                size_t overwrite_put(size_t value, size_t * result);

                // Consume method for the Bounded Buffer that supports the overwrite put
                size_t consume(size_t * result);

                // This method returns the unsafe size (difference between tail and head) of the node.
                size_t get_size() const
                {
                    auto t = _tail.load(HETCOMPUTE_BLFQ_MO(hetcompute::mem_order_relaxed));
                    auto h = _head.load(HETCOMPUTE_BLFQ_MO(hetcompute::mem_order_relaxed));
                    return (t - h);
                }

                // This method returns _logsz, which is used to determine the size of the new BLFQ to be allocated.
                size_t get_log_array_size() const
                {
                    return _logsz;
                }

                // This method returns _max_array_size, which is used to determine the maximum number of elements in a node
                size_t get_max_array_size() const
                {
                    return _max_array_size;
                }

            private:
                // Head index of the array.
                std::atomic<size_t> _head;

                // Tail index of the array. Composite word containing two fields: closed: 1 bit; t: 63 bits
                std::atomic<size_t> _tail;

                // Log(Size of the array). The number of entries in the array is 2^_logsz
                size_t const _logsz;

                //  Actual number of entries in the array.
                size_t const _max_array_size;

                std::atomic<element>* _array;
            };

            template<typename T, bool FITS_IN_SIZE_T>
            class blfq_size_t
            {
                // true and false specializations given below
            };

            // Specialize the queue for the case when the sizeof(T) > sizeof(size_t)
            template <typename T>
            class blfq_size_t<T, false>
            {
                static_assert(sizeof(T*) <= sizeof(size_t), "Error. Address has size greater than sizeof(size_t)");

            public:
                explicit blfq_size_t(size_t log_size = 12) : _blfq(log_size)
                {
                }

                // Disallow copy construction, move construction and the overloaded assignment operators.
                HETCOMPUTE_DELETE_METHOD(blfq_size_t(blfq_size_t const&));
                HETCOMPUTE_DELETE_METHOD(blfq_size_t(blfq_size_t&&));
                HETCOMPUTE_DELETE_METHOD(blfq_size_t& operator=(blfq_size_t const&));
                HETCOMPUTE_DELETE_METHOD(blfq_size_t& operator=(blfq_size_t&&));

                size_t push(T const& value, bool is_unbounded_queue)
                {
                    // Allocated space to store value
                    // TODO: Allow recycling of ptr so that we do not allocate T objects each time.
                    T* ptr = new T(value);
                    if (is_unbounded_queue)
                        return (_blfq.unbounded_put(reinterpret_cast<size_t>(ptr)));
                    else
                        return (_blfq.bounded_put(reinterpret_cast<size_t>(ptr)));
                }

                // gcc is very aggressive in trying to detect alias violations.
                HETCOMPUTE_GCC_IGNORE_BEGIN("-Wstrict-aliasing")
                size_t produce(T const& value, T& result)
                {
                    T*     ptr = new T(value);
                    T*     ret_ptr;
                    size_t res = _blfq.overwrite_put(reinterpret_cast<size_t>(ptr), reinterpret_cast<size_t*>(&ret_ptr));
                    if (res != 0)
                    {
                        if (ret_ptr != nullptr)
                        { // overwrote an existing value
                            result = *ret_ptr;
                            delete ret_ptr;
                        }
                    }
                    return res;
                }

                size_t pop(T& result, bool is_unbounded_queue)
                {
                    T*     ptr;
                    size_t res = _blfq.take(reinterpret_cast<size_t*>(&ptr), is_unbounded_queue);
                    if (res != 0)
                    {
                        result = *ptr;
                        // delete the associated backing memory
                        delete ptr;
                    }
                    return res;
                }

                size_t consume(T& result)
                {
                    T*     ptr;
                    size_t res = _blfq.consume(reinterpret_cast<size_t*>(&ptr));
                    if (res != 0)
                    {
                        result = *ptr;
                        // delete the associated backing memory
                        delete ptr;
                    }
                    return res;
                }
                HETCOMPUTE_GCC_IGNORE_END("-Wstrict-aliasing")

                void   close()
                {
                    _blfq.lock_node();
                }

                size_t size() const
                {
                    return _blfq.get_size();
                }

                size_t get_log_array_size() const
                {
                    return _blfq.get_log_array_size();
                }

                size_t get_max_array_size() const
                {
                    return _blfq.get_max_array_size();
                }

            private:
                bounded_lfqueue _blfq;
            };

            template <typename T>
            struct blfq_convert
            {
                static_assert(sizeof(T) <= sizeof(size_t), "Error. sizeof(T) > sizeof(size_t)");

                // gcc is very aggressive in trying to detect alias violations. u is local.
                HETCOMPUTE_GCC_IGNORE_BEGIN("-Wstrict-aliasing")
                static size_t cast_from(T v)
                {
                    size_t u;
                    *(reinterpret_cast<T*>(&u)) = v;
                    return u;
                }

                static T cast_to(size_t v) { return (*(reinterpret_cast<T*>(&v))); }
                HETCOMPUTE_GCC_IGNORE_END("-Wstrict-aliasing")
            };

            template <typename T>
            struct blfq_convert<T*>
            {
                static_assert(sizeof(T*) <= sizeof(size_t), "Error. sizeof(T*) > sizeof(size_t)");

                static size_t cast_from(T* v) { return reinterpret_cast<size_t>(v); }
                static T* cast_to(size_t v) { return reinterpret_cast<T*>(v); }
            };

            // Specialize the queue for the case when the sizeof(T) <= sizeof(size_t)
            template <typename T>
            class blfq_size_t<T, true>
            {
            public:
                typedef T value_type;

                blfq_size_t<T, true>(size_t log_size = 12) : _blfq(log_size)
                {
                }

                // Disallow copy construction, move construction and the overloaded assignment operators.
                HETCOMPUTE_DELETE_METHOD(blfq_size_t(blfq_size_t const&));
                HETCOMPUTE_DELETE_METHOD(blfq_size_t(blfq_size_t&&));
                HETCOMPUTE_DELETE_METHOD(blfq_size_t& operator=(blfq_size_t const&));
                HETCOMPUTE_DELETE_METHOD(blfq_size_t& operator=(blfq_size_t&&));

                size_t push(value_type const& value, bool is_unbounded_queue)
                {
                    if (is_unbounded_queue)
                        return (_blfq.unbounded_put(blfq_convert<T>::cast_from(value)));
                    else
                        return (_blfq.bounded_put(blfq_convert<T>::cast_from(value)));
                }

                // Pushes value into the queue. Stores overwritten entry in result (if any).
                size_t produce(value_type const& value, value_type& result)
                {
                    size_t tmp;
                    size_t res = _blfq.overwrite_put(blfq_convert<T>::cast_from(value), &tmp);
                    if (res != 0)
                    {
                        result = blfq_convert<T>::cast_to(tmp);
                    }
                    return (res);
                }

                size_t pop(value_type& result, bool is_unbounded_queue)
                {
                    size_t tmp;
                    size_t res = _blfq.take(&tmp, is_unbounded_queue);
                    if (res != 0)
                    {
                        result = blfq_convert<T>::cast_to(tmp);
                    }
                    return res;
                }

                size_t consume(value_type& result)
                {
                    size_t tmp;
                    size_t res = _blfq.consume(&tmp);
                    if (res != 0)
                    {
                        result = blfq_convert<T>::cast_to(tmp);
                    }
                    return res;
                }

                void   close()
                {
                    _blfq.lock_node();
                }

                size_t size() const
                {
                    return _blfq.get_size();
                }

                size_t get_log_array_size() const
                {
                    return _blfq.get_log_array_size();
                }

                size_t get_max_array_size() const
                {
                    return _blfq.get_max_array_size();
                }

            private:
                bounded_lfqueue _blfq;
            };

        }; // namespace blfq
    }; // namespace internal
}; // namespace hetcompute
