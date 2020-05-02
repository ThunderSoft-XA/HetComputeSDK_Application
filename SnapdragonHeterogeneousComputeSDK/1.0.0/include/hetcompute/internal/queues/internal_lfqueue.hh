/** @file lfqueue.hh*/
#pragma once

#include <atomic>

#include <hetcompute/internal/queues/internal_bounded_lfqueue.hh>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/macros.hh>
#include <hetcompute/internal/util/memorder.hh>

#if HETCOMPUTE_LFQ_FORCE_SEQ_CST
    #define HETCOMPUTE_LFQ_MO(x) hetcompute::mem_order_seq_cst
#else
    #define HETCOMPUTE_LFQ_MO(x) x
#endif

namespace hetcompute
{
    namespace internal
    {
        namespace lfq
        {
            // Implementation of an Unbounded Lock-Free Queue (henceforth referred to as lfq or lfqueue) that
            // provides FIFO semantics

            // Forward declaration
            template<typename T> class lfq;

            // A node in the lfqueue and bounded buffer. The lfqueue is a linked list of these nodes.
            // It contains a bounded lfqueue as well as a pointer to the next node in the linked list
            template<typename T>
            class lfq_node
            {
            public:
                typedef typename internal::blfq::blfq_size_t<T, sizeof(T) <= sizeof(size_t) > container_type;

                explicit lfq_node(size_t log_size) : _c(log_size), _next(nullptr)
                {
                }

                // Disallow copy construction, move construction and the overloaded assignment operators
                HETCOMPUTE_DELETE_METHOD(lfq_node(lfq_node const &));
                HETCOMPUTE_DELETE_METHOD(lfq_node(lfq_node &&));
                HETCOMPUTE_DELETE_METHOD(lfq_node& operator=(lfq_node const &));
                HETCOMPUTE_DELETE_METHOD(lfq_node& operator=(lfq_node &&));

                // Return the current value of the next pointer of a node.
                lfq_node<T>* get_next()
                {
                    return _next.load(HETCOMPUTE_LFQ_MO(hetcompute::mem_order_relaxed));
                }

                // Update the next pointer of a node to point to a newly created node.
                // returns true if the pointer was successfully updated; false otherwise.
                bool set_next(lfq_node<T>* const & new_val)
                {
                    // Update next pointer to point to the new lfq node.
                    lfq_node<T>* old_val = nullptr;
                    if (_next.compare_exchange_strong(old_val, new_val, HETCOMPUTE_LFQ_MO(hetcompute::mem_order_relaxed)))
                    {
                        return true;
                    }
                    // CAS failed.
                    return false;
                }

                size_t get_log_node_size() const
                {
                    return _c.get_log_array_size();
                }

                size_t get_max_node_size() const
                {
                    return _c.get_max_array_size();
                }

            private:
                // The bounded lfqueue
                container_type _c;

                // A pointer to the next node in the linked list.
                std::atomic <lfq_node<T>*> _next;

                friend class lfq<T>;
            };

            // The internal lfqueue class.
            template<typename T>
            class lfq
            {
            public:
                typedef lfq_node<T> blfq_node;
                typedef T value_type;

                explicit lfq(size_t log_size = 12) : _head(nullptr), _tail(nullptr), _original_head(nullptr)
                {
                    _head = new blfq_node(log_size);
                    _tail.store(_head);
                    _original_head = _head;
                }

                virtual ~lfq()
                {
                    // Free All allocated nodes in the queue, since its construction.
                    HETCOMPUTE_INTERNAL_ASSERT(_original_head != nullptr, "Error. Head is nullptr");
                    while (_original_head != nullptr)
                    {
                        auto cur       = _original_head;
                        _original_head = cur->_next;
                        delete cur;
                    }
                }

                // Disallow copy construction, move construction and the overloaded assignment operators
                HETCOMPUTE_DELETE_METHOD(lfq(lfq const&));
                HETCOMPUTE_DELETE_METHOD(lfq(lfq &&));
                HETCOMPUTE_DELETE_METHOD(lfq& operator=(lfq const&));
                HETCOMPUTE_DELETE_METHOD(lfq& operator=(lfq &&));

                // Push method. Value is pushed into the BLFQ of the node pointed to by _tail.
                // If the BLFQ is full, then a new node is created and the value is pushed into the BLFQ of
                // that node. Finally, the node is appended to the list. If the CAS to append the node fails,
                // then the push operation is retried. Push is always successful, so returns a value >= 1
                size_t push(value_type const & v)
                {
                    // Keep track of the number of times a full node is encountered
                    // This value is then multiplied with the size of a node to return an estimate of the size of
                    // the number of elements of the queue.
                    size_t full_nodes_encountered = 0;

                    while (true)
                    {
                        auto current_blfq_node = _tail.load(HETCOMPUTE_LFQ_MO(hetcompute::mem_order_relaxed));

                        // First check if _tail needs to be updated.
                        auto next_blfq_node = current_blfq_node->get_next();

                        // _tail needs to be updated. Perform update using a CAS instruction, and restart.
                        if (next_blfq_node != nullptr)
                        {
                            _tail.compare_exchange_strong(current_blfq_node, next_blfq_node, HETCOMPUTE_LFQ_MO(hetcompute::mem_order_relaxed));
                            continue;
                        }

                        // Try to push into current_blfq. Return if successful.
                        auto sz = current_blfq_node->_c.push(v, true);
                        if (sz != 0)
                        {
                            return (sz + (full_nodes_encountered * current_blfq_node->get_max_node_size()));
                        }
                        else
                        {
                            // node was full.
                            // update count of full nodes encountered.
                            full_nodes_encountered++;
                        }

                        // Push failed. Allocate a new node, and push value into the BLFQ of that node.
                        auto new_blfq_node = new blfq_node(current_blfq_node->get_log_node_size());
                        // Push into the newly allocated node. Note that this is 'local' push in the sense that
                        // only the current thread is aware of this node. In other words, there is no contention here.
                        sz = new_blfq_node->_c.push(v, true);
                        HETCOMPUTE_INTERNAL_ASSERT(sz != 0, "Push into a local lfq_node. Should always succeed");

                        // Try to append that node to the end of the current LFQ.
                        if (current_blfq_node->set_next(new_blfq_node))
                        {
                            // Successfully appended node to the end of the LFQ. Now update _tail to point to this node.
                            // This is a lock-free algorithm, and hence this step may be performed by another thread,
                            // who finds next_blfq != nullptr.
                            _tail.compare_exchange_strong(current_blfq_node, new_blfq_node, HETCOMPUTE_LFQ_MO(hetcompute::mem_order_relaxed));
                            return (sz + (full_nodes_encountered * current_blfq_node->get_max_node_size()));
                        }
                        else
                        {
                            // Failed to append new_blfq_node. Delete it.
                            delete new_blfq_node;
                        }
                    }
                }

                // Pop method. Element is removed from the BLFQ at the node pointed to by _head.
                // Returns 0 if the LFQ is empty; unsafe size of the head node otherwise.
                size_t pop(value_type & r)
                {
                    while (true)
                    {
                        auto current_blfq_node = _head.load(HETCOMPUTE_LFQ_MO(hetcompute::mem_order_relaxed));
                        auto sz                = current_blfq_node->_c.pop(r, true);
                        if (sz != 0)
                        {
                            return sz;
                        }

                        // pop failed.
                        auto next_blfq_node = current_blfq_node->get_next();
                        if (next_blfq_node == nullptr)
                        {
                            // queue empty
                            return sz;
                        }

                        // Again check current_blfq_node. Note that at this point the node is closed, since a new node
                        // has already been appended to it. Doing another pop ensures that a slow enqueuer's push
                        // would either be popped, or will force the enqueuer to restart after determining that the node is closed.
                        // Consider the following scenario in a blfq_node of size 256:
                        // 1. 256 enqueuers come and successfully push their values.
                        // 2. Enqueuers 257 and 258 start.
                        // 3. Enqueuer 257 finds the node full, but before he can set closed to 1, enqueuer 258 reads
                        // it and finds closed to be 0.
                        // 4. Now 256 dequeuers arrive and pop all items from the node.
                        // 5. Dequeuer 257 arrives and executes the first pop() in this method. It returns empty.
                        // 6. Now enqueuer 257 appends the new node.
                        // 7. The dequeuer now checks line 130 (above) and finds that a new node is appended.
                        // 8. By checking below (line 150 of this method) - Dequeuer 258 will either pop the value pushed by
                        // enqueuer 258, or will cause enqueuer 258 to restart the while loop,
                        // at which point it will find that the node is closed.
                        sz = current_blfq_node->_c.pop(r, true);
                        if (sz != 0)
                        {
                            return sz;
                        }

                        // Pop failed, so the node is empty. Switch head to point to next_blfq
                        _head.compare_exchange_strong(current_blfq_node, next_blfq_node, HETCOMPUTE_LFQ_MO(hetcompute::mem_order_relaxed));
                    }
                }

                // Size of the queue. Since the queue is a linked list of nodes, and the size is used only to determine if
                // waiting threads need to be signaled, it is sufficient to return the size of the head node.
                // Return number of elements in the (head node of the) queue.
                // This is the unsafe size, and is used only for debugging purposes, i.e dumping the log.
                // This method is not exposed to the user.
                size_t head_node_size() const
                {
                    auto current_blfq_node = _head.load(HETCOMPUTE_LFQ_MO(hetcompute::mem_order_relaxed));
                    return current_blfq_node->_c.size();
                }

            private:
                // Head of the queue. This is the node from which values are popped.
                std::atomic<blfq_node*> _head;

                // Tail of the queue. This is the node into which values are pushed.
                std::atomic<blfq_node*> _tail;

                // Original head of the list of allocated nodes. Used at the time of destruction to free all allocated nodes.
                blfq_node* _original_head;
            };

        }; // namespace lfq

    }; // namespace internal

}; // namespace hetcompute
