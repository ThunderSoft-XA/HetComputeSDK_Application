#pragma once

#include <hetcompute/internal/log/log.hh>

namespace hetcompute
{
    namespace internal
    {
        // forwarding declaration
        namespace testing
        {
            class pool_tests;
        }; // namespace testing

        /**
        This class implements a thread-safe pool for objects of class T. The
        pool is implemented as a linked list of slabs. Each slab allocates
        Size_ objects. When a slab gets full, it creates a new one that gets
        added to the list.

        This class makes the following assumptions:

        - Objects are never deleted (thus the pool can only grow)

        - The slab size must be larger than the number of concurrent threads
        accesing the pool (MAX_SHARERS).
        */
        template <typename T, size_t Size_>
        class node_pool
        {
        public:
            typedef T          node_type;
            typedef size_t     size_type;
            typedef node_type* pointer;

        private:
            /**
                A slab is a bunch of tree nodes and
                a pointer to the next slab
            */
            class node_slab
            {
                node_type               _buffer[Size_];
                const size_type         _first_pos;
                std::atomic<node_slab*> _next;

            public:
                // Constructor
                explicit node_slab(size_type first_pos) : _first_pos(first_pos), _next(nullptr) {}

                size_type begin() const { return _first_pos; }
                size_type end() const { return begin() + Size_; }

                bool has(size_type pos) const { return ((pos >= begin()) && (pos < end())); }

                void set_next(node_slab* next, hetcompute::mem_order order) { _next.store(next, order); }

                node_slab* get_next(hetcompute::mem_order order) { return _next.load(order); }

                pointer get_ptr(size_type pos) { return &_buffer[pos]; }

                // Disable all copying and movement.
                HETCOMPUTE_DELETE_METHOD(node_slab(node_slab const&));
                HETCOMPUTE_DELETE_METHOD(node_slab(node_slab&&));
                HETCOMPUTE_DELETE_METHOD(node_slab& operator=(node_slab const&));
                HETCOMPUTE_DELETE_METHOD(node_slab& operator=(node_slab&&));

                ~node_slab() {}
            };  // class node_slab

            node_slab               _inlined_slab;
            std::atomic<size_type>  _pos;
            std::atomic<node_slab*> _slab;

            // Returns slab that includes pos, or nullptr if none
            node_slab* find_slab(const size_type pos, node_slab* first_slab)
            {
                auto ck = first_slab;
                while (ck != nullptr)
                {
                    if (ck->has(pos))
                    {
                        return ck;
                    }
                    ck = ck->get_next(hetcompute::mem_order_acquire);
                }
                return nullptr;
            }

            // This method creates a new slab and attaches it to the slab linked list
            node_slab* grow(size_type pos, node_slab* current_slab)
            {
                HETCOMPUTE_INTERNAL_ASSERT(current_slab, "null ptr");

                // Corner case. I'd doubt that it would ever execute
                if (pos == 0)
                {
                    return get_inlined_slab();
                }

                // Allocate new slab
                node_slab* new_slab = new node_slab(pos);

                // We hope that current_slab is up_to_date and that
                // it is the right predecessor. If not, go find the
                // right predecessing slab.
                size_type current_end = current_slab->end();
                if (current_end != pos)
                {
                    current_slab = find_slab(pos - 1, get_inlined_slab());
                    HETCOMPUTE_INTERNAL_ASSERT(current_slab != nullptr, "null ptr");
                }

                HETCOMPUTE_INTERNAL_ASSERT(current_slab->has(pos - 1), "Wrong slab");

                // Update next pointer
                current_slab->set_next(new_slab, hetcompute::mem_order_relaxed);
                // Store new slab. Notice that we are using release to write the
                // new slab pointer. This guarantees that old_slab->_next is
                // updated before _slab becomes visible.
                _slab.store(new_slab, hetcompute::mem_order_release);
                log::fire_event<log::events::ws_tree_new_slab>();
                return new_slab;
            }

            pointer get_next_impl()
            {
                // We first read both pos and slab. We can use relaxed
                // atomics for both because if we read the wrong slab
                // we'll be able to detect it and fix it
                auto pos  = _pos.fetch_add(size_type(1), hetcompute::mem_order_relaxed);
                auto slab = _slab.load(hetcompute::mem_order_relaxed);

                // This is the position within the slab. Notice that this
                // position is the same for all slabs
                const auto pos_in_slab = pos % Size_;

                // _slab can't be NULL because we initialize it with
                // the address of the inlined slab.
                HETCOMPUTE_INTERNAL_ASSERT(slab != nullptr, "Invalid ptr null");

                // This is the common case: the current pos is within the
                // boundaries of the slab we observed
                if (slab->has(pos))
                {
                    return slab->get_ptr(pos_in_slab);
                }

                // So something is not what we expected. Perhaps we need
                // a new slab because we ran out of room in the current one
                // or we read an old slab. In any case, we read the most
                // up-to-date slab.
                slab = _slab.load(hetcompute::mem_order_acquire);

                if (pos_in_slab == 0)
                {
                    // We have no room left in the current slab, and since we are
                    // the first ones to figure this out, we are in charge of
                    // growing the pool. Notice that at this point _slab cannot
                    // change because of our rule that limits the number of
                    // concurrent accesses.
                    return grow(pos, slab)->get_ptr(0);
                }

                // We now try to find our slab. We might have to spin a little
                // bit if the slab we are supposed to return is not there yet.
                do
                {
                    slab = find_slab(pos, get_inlined_slab());
                } while (!slab);

                HETCOMPUTE_INTERNAL_ASSERT(slab->has(pos), "Wrong slab");

                return slab->get_ptr(pos_in_slab);
            }

        public:
            explicit node_pool(size_type max_sharers) : _inlined_slab(0), _pos(0), _slab(&_inlined_slab)
            {
                HETCOMPUTE_INTERNAL_ASSERT(max_sharers < Size_, "Too many sharers for pool.");
            }

            //  Returns pointer to the next available memory location.
            //  This is thread safe. If there are no more memory available
            //  available in the current slab, it'll create a new one
            pointer get_next()
            {
                auto next = get_next_impl();
                HETCOMPUTE_API_ASSERT(next, "Could not allocate slab.");
                log::fire_event<log::events::ws_tree_node_created>();
                return next;
            }

            // Returns pointer to inlined slab
            node_slab* get_inlined_slab() { return &_inlined_slab; }

            // Destructor.
            // Not thread-safe. It is the user's responsibility to
            // make sure that no clients are accessing it
            // while it runs
            ~node_pool()
            {
                auto next_slab = _inlined_slab.get_next(hetcompute::mem_order_acquire);
                while (next_slab)
                {
                    auto tmp = next_slab->get_next(hetcompute::mem_order_relaxed);
                    delete next_slab;
                    next_slab = tmp;
                }
            };

            // Disable all copying and movement.
            HETCOMPUTE_DELETE_METHOD(node_pool(node_pool const&));
            HETCOMPUTE_DELETE_METHOD(node_pool(node_pool&&));
            HETCOMPUTE_DELETE_METHOD(node_pool& operator=(node_pool const&));
            HETCOMPUTE_DELETE_METHOD(node_pool& operator=(node_pool&&));

        };  // class node_pool

    };  // namespace internal
};  // namespace hetcompute
