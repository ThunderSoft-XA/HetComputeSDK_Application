#pragma once

#include <hetcompute/internal/patterns/workstealtree/wstree.hh>

namespace hetcompute
{
    namespace internal
    {
        // Define all the dynamic policies here
        enum class Policy : uint8_t
        {
            REDUCE = 0,
            MAP    = 1,
            SCAN   = 2,
        };

        /**
         *
         * This is the base class for adaptive work stealing strategy. It includes
         * members that are common to all pattern-specific adaptive strategies.
         */
        template <typename Fn>
        class adaptive_strategy_base
        {
        public:
            typedef typename function_traits<Fn>::type_in_task f_type;

            adaptive_strategy_base(group_ptr g, Fn&& f, legacy::task_attrs attrs) : _group(g), _f(f), _task_attrs(attrs), _prealloc(false)
            {
            }

            group_ptr          get_group() { return _group; }
            Fn&                get_fn() { return _f; }
            bool               is_prealloc() { return _prealloc; }
            legacy::task_attrs get_task_attrs() const { return _task_attrs; }

        protected:
            group_ptr          _group;
            f_type             _f;
            legacy::task_attrs _task_attrs;
            // prealloc refers to the optimization of constructing the tree in advance
            // to avoid some initial stealing activities. For example, if there are 4
            // tasks working on the range, the tree can be constructed to be a 3-level
            // full binary tree with 4 leaves. Each task will get one of the leaves to
            // work on initially without incuring expensive stealing overhead upfront.
            bool _prealloc;

        private:
            // Disable all copying and movement.
            HETCOMPUTE_DELETE_METHOD(adaptive_strategy_base(adaptive_strategy_base const&));
            HETCOMPUTE_DELETE_METHOD(adaptive_strategy_base(adaptive_strategy_base&&));
            HETCOMPUTE_DELETE_METHOD(adaptive_strategy_base& operator=(adaptive_strategy_base const&));
            HETCOMPUTE_DELETE_METHOD(adaptive_strategy_base& operator=(adaptive_strategy_base&&));
        };  // class adaptive_strategy_base


        // Base class for common stealing tree operations
        template <typename T>
        class tree_ops_base
        {
        public:
            typedef size_t     size_type;
            typedef ws_tree<T> tree_type;
            typedef ws_node<T> work_item_type;

            // enable only for preduce
            template <typename SizeType, typename U, typename = typename std::enable_if<!std::is_same<U, void>::value>::type>
            tree_ops_base(SizeType first, SizeType last, SizeType blk_size, const U& identity, const hetcompute::pattern::tuner& tuner)
                : _workstealtree(first, last, blk_size, identity, tuner.get_doc())
            {
            }

            // enable only for pfor_each
            template <typename SizeType, typename U, typename = typename std::enable_if<std::is_same<U, void>::value>::type>
            tree_ops_base(SizeType first, SizeType last, SizeType blk_size, SizeType stride, const hetcompute::pattern::tuner& tuner)
                : _workstealtree(first, last, blk_size, stride, tuner.get_doc())
            {
            }

            // overload for pscan
            tree_ops_base(size_t first, size_t last, size_t blk_size, const hetcompute::pattern::tuner& tuner)
                : _workstealtree(first, last, blk_size, T(), tuner.get_doc())
            {
            }

            // common operations on tree

            // Degree of concurrency, see workstealtree/wstree_base.hh for details
            size_type get_max_tasks() const { return _workstealtree.get_max_tasks(); }
            size_type get_blk_size() const { return _workstealtree.get_blk_size(); }

            // we don't expose tree so we wrap find_work_prealloc
            work_item_type* find_work_prealloc(size_type task_id) { return _workstealtree.find_work_prealloc(task_id); }

            work_item_type* get_root() const { return _workstealtree.get_root(); }
            size_type       get_prealloc_leaf() { return _workstealtree.get_leaf_num(); }

#ifdef HETCOMPUTE_ADAPTIVE_PFOR_DEBUG
            const tree_type& get_tree() const { return _workstealtree; }
#endif // HETCOMPUTE_ADAPTIVE_PFOR_DEBUG

        protected:
            tree_type _workstealtree;

        private:
            // Disable all copying and movement.
            HETCOMPUTE_DELETE_METHOD(tree_ops_base(tree_ops_base const&));
            HETCOMPUTE_DELETE_METHOD(tree_ops_base(tree_ops_base&&));
            HETCOMPUTE_DELETE_METHOD(tree_ops_base& operator=(tree_ops_base const&));
            HETCOMPUTE_DELETE_METHOD(tree_ops_base& operator=(tree_ops_base&&));
        };  // class tree_ops_base

        // Primary template for adaptive stealing strategy
        // This class abstracts all the key elements to process data-parallel
        // operations with load balancing. Basic elements include the group
        // to synchronize all the tasks, the range to work on, and the task body.
        // There are some details that are pattern-dependent. Hence, we provide
        // specialization to optimize for each case.
        //
        // type T               for local aggregate result
        //                      used by preduce and pscan
        // type InputIterator   for access container element
        //                      used by pscan
        // type Fn              for kernel function passed to adaptive stealing
        //                      used by all
        // type BinaryFn        untransformed kernel function for updating local sum
        //                      used by pscan
        // type Policy          specialized for different strategy type
        template <typename T, typename InputIterator, typename Fn, typename BinaryFn, Policy P>
        class adaptive_steal_strategy;

        // Specialize for preduce
        template <typename T, typename Fn>
        class adaptive_steal_strategy<T, size_t, Fn, Fn, Policy::REDUCE>
            : public adaptive_strategy_base<Fn>,
              public tree_ops_base<T>,
              public ref_counted_object<adaptive_steal_strategy<T, size_t, Fn, Fn, Policy::REDUCE>>
        {
        public:
            typedef size_t     size_type;
            typedef ws_tree<T> tree_type;
            typedef ws_node<T> work_item_type;

            adaptive_steal_strategy(group_ptr                       g,
                                    size_type                       first,
                                    size_type                       last,
                                    const T&                        identity,
                                    Fn&&                            f,
                                    legacy::task_attrs              attrs,
                                    size_type                       blk_size,
                                    const hetcompute::pattern::tuner& tuner)
                : adaptive_strategy_base<Fn>(g, std::forward<Fn>(f), attrs),
                  tree_ops_base<T>(first, last, blk_size, identity, tuner),
                  _identity(std::move(identity))
            {
            }

            void static_split(size_type max_tasks, const T& identity)
            {
                tree_ops_base<T>::_workstealtree.split_tree_before_stealing(max_tasks, identity);
                adaptive_strategy_base<Fn>::_prealloc = true;
            }

            // we don't expose tree so we wrap find_work_intree
            // have to pass identity to try_steal
            work_item_type* find_work_intree(work_item_type* n, size_type blk_size, const T& identity)
            {
                return tree_ops_base<T>::_workstealtree.find_work_intree(n, blk_size, identity);
            }

            const T& get_identity() const { return _identity; }

        private:
            const T _identity;

            // Disable all copying and movement.
            HETCOMPUTE_DELETE_METHOD(adaptive_steal_strategy(adaptive_steal_strategy const&));
            HETCOMPUTE_DELETE_METHOD(adaptive_steal_strategy(adaptive_steal_strategy&&));
            HETCOMPUTE_DELETE_METHOD(adaptive_steal_strategy& operator=(adaptive_steal_strategy const&));
            HETCOMPUTE_DELETE_METHOD(adaptive_steal_strategy& operator=(adaptive_steal_strategy&&));
        };  // class adaptive_steal_strategy

        // Specialize for pscan
        // Pscan implementation on multi-core involves several stages
        // The strategy specialized for pscan includes several conditional
        // variables to synchronize execution of stealer tasks
        HETCOMPUTE_GCC_IGNORE_BEGIN("-Weffc++");
        template <typename T, typename InputIterator, typename Fn, typename BinaryFn>
        class adaptive_steal_strategy<T, InputIterator, Fn, BinaryFn, Policy::SCAN>
            : public adaptive_strategy_base<Fn>,
              public tree_ops_base<T>,
              public ref_counted_object<adaptive_steal_strategy<T, InputIterator, Fn, BinaryFn, Policy::SCAN>>
        {
            HETCOMPUTE_GCC_IGNORE_END("-Weffc++");

        public:
            typedef size_t               size_type;
            typedef ws_tree<T>           tree_type;
            typedef InputIterator        iter_type;
            typedef ws_node<T>           work_item_type;
            typedef uint8_t              counter_type;
            typedef std::atomic<uint8_t> atomic_counter_type;
            typedef std::atomic<bool>    atomic_bool_type;

            adaptive_steal_strategy(group_ptr                       g,
                                    InputIterator                   base,
                                    size_type                       first,
                                    size_type                       last,
                                    Fn&&                            f,
                                    BinaryFn&&                      bf,
                                    legacy::task_attrs              attrs,
                                    size_type                       blk_size,
                                    const hetcompute::pattern::tuner& tuner)
                : adaptive_strategy_base<Fn>(g, std::forward<Fn>(f), attrs),
                  tree_ops_base<T>(first, last, blk_size, tuner),
                  _bf(bf),
                  _base(base),
                  _task_counter(0),
                  _lsum_update_done(false)
            {
            }

            BinaryFn& get_binary_fn() { return _bf; }
            iter_type get_base() { return _base; }

            void static_split(size_type max_tasks)
            {
                tree_ops_base<T>::_workstealtree.split_tree_before_stealing(max_tasks);
                adaptive_strategy_base<Fn>::_prealloc = true;
            }

            work_item_type* find_work_intree(size_type tid, work_item_type* n, size_type blk_size)
            {
                return tree_ops_base<T>::_workstealtree.find_work_intree(tid, n, blk_size);
            }

            void inc_task_counter(hetcompute::mem_order order = hetcompute::mem_order_seq_cst) { _task_counter.fetch_add(1, order); }

            uint8_t get_task_counter(hetcompute::mem_order order = hetcompute::mem_order_seq_cst) { return _task_counter.load(order); }

            void set_local_sum_updated(hetcompute::mem_order order = hetcompute::mem_order_seq_cst) { _lsum_update_done.store(true, order); }

            bool is_local_sum_updated(hetcompute::mem_order order = hetcompute::mem_order_seq_cst) { return _lsum_update_done.load(order); }

        private:
            BinaryFn  _bf;                      // scan operation
            iter_type _base;                    // iterator to the head of the
                                                // sequence
            atomic_counter_type _task_counter;  // num of tasks completed in local
                                                // range scan
            atomic_bool_type _lsum_update_done; // state variable representing if the
                                                // master task completes updating
                                                // of local sum with pre-order
                                                // traversal

            // Disable all copying and movement.
            HETCOMPUTE_DELETE_METHOD(adaptive_steal_strategy(adaptive_steal_strategy const&));
            HETCOMPUTE_DELETE_METHOD(adaptive_steal_strategy(adaptive_steal_strategy&&));
            HETCOMPUTE_DELETE_METHOD(adaptive_steal_strategy& operator=(adaptive_steal_strategy const&));
            HETCOMPUTE_DELETE_METHOD(adaptive_steal_strategy& operator=(adaptive_steal_strategy&&));
        };

        // Speciliazation for pfor
        HETCOMPUTE_GCC_IGNORE_BEGIN("-Weffc++");
        template <typename Fn>
        class adaptive_steal_strategy<void, size_t, Fn, Fn, Policy::MAP>
            : public adaptive_strategy_base<Fn>,
              public tree_ops_base<void>,
              public ref_counted_object<adaptive_steal_strategy<void, size_t, Fn, Fn, Policy::MAP>>
        {
            HETCOMPUTE_GCC_IGNORE_END("-Weffc++");

        public:
            typedef size_t        size_type;
            typedef ws_tree<void> tree_type;
            typedef ws_node<void> work_item_type;

            adaptive_steal_strategy(group_ptr                       g,
                                    size_type                       first,
                                    size_type                       last,
                                    Fn&&                            f,
                                    legacy::task_attrs              attrs,
                                    size_type                       blk_size,
                                    size_type                       stride,
                                    const hetcompute::pattern::tuner& tuner)
                : adaptive_strategy_base<Fn>(g, std::forward<Fn>(f), attrs), tree_ops_base<void>(first, last, blk_size, stride, tuner)
            {
            }

            size_type get_stride() const { return _workstealtree.get_stride(); }

            void static_split(size_type max_tasks)
            {
                _workstealtree.split_tree_before_stealing(max_tasks);
                adaptive_strategy_base<Fn>::_prealloc = true;
            }

            // we don't expose tree so we wrap find_work_intree
            work_item_type* find_work_intree(work_item_type* n, size_type blk_size) { return _workstealtree.find_work_intree(n, blk_size); }

        private:
            // Disable all copying and movement.
            HETCOMPUTE_DELETE_METHOD(adaptive_steal_strategy(adaptive_steal_strategy const&));
            HETCOMPUTE_DELETE_METHOD(adaptive_steal_strategy(adaptive_steal_strategy&&));
            HETCOMPUTE_DELETE_METHOD(adaptive_steal_strategy& operator=(adaptive_steal_strategy const&));
            HETCOMPUTE_DELETE_METHOD(adaptive_steal_strategy& operator=(adaptive_steal_strategy&&));
        };

    }; // namespace internal
};     // namespace hetcompute
