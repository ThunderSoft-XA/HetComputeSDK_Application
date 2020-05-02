#pragma once

#include <cstdlib>
#include <hetcompute/internal/util/memorder.hh>

namespace hetcompute
{
    namespace internal
    {
        enum class try_steal_result : uint8_t
        {
            SUCCESS          = 0,
            ALREADY_FINISHED = 1,
            ALREADY_STOLEN   = 2,
            NULL_POINTER     = 3,
            INVALID          = 4,
        };

        /**
        Base class of ws_node which extracts out common data members used by
        various ws_node versions. Currently we restrict class hierarchy to two
        levels. If in the future we are further deriving from the specific node
        class, we have to resort to virtual inheritance.
        */

        class ws_node_base
        {
        public:
            typedef uint16_t counter_type;
            typedef size_t   size_type;

            static const size_type UNCLAIMED;
            static const size_type STOLEN;
            static const intptr_t  NODE_COMPLETE_BIT;

        protected:
            typedef std::atomic<size_type> atomic_size_type;

#ifdef HETCOMPUTE_ADAPTIVE_PFOR_DEBUG
            atomic_size_type _traversal;
            size_type        _worker_id;
            size_type        _progress_save;
#endif // HETCOMPUTE_ADAPTIVE_PFOR_DEBUG

            counter_type     _left_traversal;
            counter_type     _right_traversal;
            const size_type  _first;
            const size_type  _last;
            atomic_size_type _progress;
            bool             _pre_assigned;

            // Disable all copying and movement.
            HETCOMPUTE_DELETE_METHOD(ws_node_base(ws_node_base const&));
            HETCOMPUTE_DELETE_METHOD(ws_node_base(ws_node_base&&));
            HETCOMPUTE_DELETE_METHOD(ws_node_base& operator=(ws_node_base const&));
            HETCOMPUTE_DELETE_METHOD(ws_node_base& operator=(ws_node_base&&));

        public:
            // Default constructor
            ws_node_base()
                :
#ifdef HETCOMPUTE_ADAPTIVE_PFOR_DEBUG
                  _traversal(0),
                  _worker_id(0),
                  _progress_save(0),
#endif // HETCOMPUTE_ADAPTIVE_PFOR_DEBUG
                  _left_traversal(0),
                  _right_traversal(0),
                  _first(0),
                  _last(0),
                  _progress(0),
                  _pre_assigned(false)
            {
            }

            ws_node_base(size_type first, size_type last, size_type progress)
                :
#ifdef HETCOMPUTE_ADAPTIVE_PFOR_DEBUG
                  _traversal(0),
                  _worker_id(0),
                  _progress_save(0),
#endif // HETCOMPUTE_ADAPTIVE_PFOR_DEBUG
                  _left_traversal(0),
                  _right_traversal(0),
                  _first(first),
                  _last(last),
                  _progress(progress),
                  _pre_assigned(false)
            {
            }

            virtual ~ws_node_base() {}

#ifdef HETCOMPUTE_ADAPTIVE_PFOR_DEBUG
            size_type count_traversal() const { return _traversal.load(hetcompute::mem_order_relaxed); }
            void      increase_traversal(size_type n, hetcompute::mem_order order = hetcompute::mem_order_seq_cst)
            {
                _traversal.fetch_add(n, order);
            }

            size_type get_worker_id() const { return _worker_id; }
            void      set_worker_id(size_type id) { _worker_id = id; }

            size_type get_progress_save() const { return _progress_save; }
            void      set_progress_save(size_type psave) { _progress_save = psave; }
#endif // HETCOMPUTE_ADAPTIVE_PFOR_DEBUG

            bool is_unclaimed(hetcompute::mem_order order = hetcompute::mem_order_seq_cst) { return get_progress(order) == UNCLAIMED; }

            bool is_stolen(size_type blk_size, hetcompute::mem_order order = hetcompute::mem_order_seq_cst)
            {
                auto progress = get_progress(order);
                return (progress == STOLEN) || (progress == STOLEN + blk_size);
            }

            size_type get_first() const { return _first; }
            size_type get_last() const { return _last; }

            bool is_assigned() const { return _pre_assigned; }
            void set_assigned() { _pre_assigned = true; }

            size_type get_progress(hetcompute::mem_order order = hetcompute::mem_order_seq_cst) { return _progress.load(order); }

            size_type inc_progress(size_type n, hetcompute::mem_order order) { return _progress.fetch_add(n, order); }

            size_type get_left_traversal() const { return _left_traversal; }
            size_type get_right_traversal() const { return _right_traversal; }

            // Setter func for pscan
            // The leftmost 4 bits for _left_traversal are reserved for task id
            void save_task_id(size_t id) { _left_traversal += static_cast<counter_type>(id << 12); }

            // fetch task id encrypted in left_traversal
            counter_type get_task_id() const { return _left_traversal >> 12; }

            void inc_left_traversal() { _left_traversal++; }
            void inc_right_traversal() { _right_traversal++; }

        }; // class ws_node_base

        /**
        Base class for ws_tree
        The tree does not contain any node-specific information as of yet.
        */
        class ws_tree_base
        {
        public:
            typedef size_t size_type;

            explicit ws_tree_base(size_type blk_size, size_type doc)
                : _max_tasks(doc), _prealloc_leaf(1), _prealloc_level(0), _blk_size(blk_size)
            {
            }

            virtual ~ws_tree_base() {}

            size_type get_max_tasks() const { return _max_tasks; }
            size_type get_leaf_num() const { return _prealloc_leaf; }
            size_type get_level() const { return _prealloc_level; }
            size_type get_blk_size() const { return _blk_size; }

        protected:
            // Degree of concurrency, set to the number of cores on the device by default
            const size_type _max_tasks;
            size_type       _prealloc_leaf;
            size_type       _prealloc_level;
            size_type       _blk_size;

            // Disable all copying and movement.
            HETCOMPUTE_DELETE_METHOD(ws_tree_base(ws_tree_base const&));
            HETCOMPUTE_DELETE_METHOD(ws_tree_base(ws_tree_base&&));
            HETCOMPUTE_DELETE_METHOD(ws_tree_base& operator=(ws_tree_base const&));
            HETCOMPUTE_DELETE_METHOD(ws_tree_base& operator=(ws_tree_base&&));
        }; // class ws_tree_base

    }; // namespace internal
};     // namspace hetcompute
