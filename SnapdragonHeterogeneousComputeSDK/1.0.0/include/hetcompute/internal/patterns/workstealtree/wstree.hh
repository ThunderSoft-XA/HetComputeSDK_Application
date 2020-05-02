#pragma once

#include <hetcompute/runtime.hh>
#include <hetcompute/internal/patterns/workstealtree/wstnodepool.hh>
#include <hetcompute/internal/patterns/workstealtree/wstree_base.hh>

namespace hetcompute
{
    namespace internal
    {
        template <typename T>
        class ws_tree;

        /**
        This class implements the basic work unit of an adaptive worksteal tree.
        When balancing pfor workloads amongst N tasks, the complete working range
        is adaptively splitted into different sized work units. The adaptive
        splitting process is organized as a tree structure.

        Because we pre-allocate a pool of tree nodes on the tree, each node
        has a pointer to the tree and delegates construction to the tree.

        The general form of ws_node class stores intermediate value of type T
        */

        template <typename T>
        class ws_node : public ws_node_base
        {
        public:
            typedef ws_node<T> node_type;
            typedef ws_tree<T> tree_type;

        private:
            typedef ws_node<T>*                    node_type_pointer;
            typedef ws_tree<T>*                    tree_type_pointer;
            typedef std::atomic<node_type_pointer> atomic_node_type_pointer;

            atomic_node_type_pointer _left;
            atomic_node_type_pointer _right;
            tree_type_pointer        _tree;
            T                        _value;

            ws_node(size_type first, size_type last, size_type progress, tree_type_pointer tree, const T& identity)
                : ws_node_base(first, last, progress), _left(nullptr), _right(nullptr), _tree(tree), _value(std::move(identity))
            {
            }

            friend class ws_tree<T>;
            friend class testing::pool_tests;

            // Disable all copying and movement.
            HETCOMPUTE_DELETE_METHOD(ws_node(ws_node const&));
            HETCOMPUTE_DELETE_METHOD(ws_node(ws_node&&));
            HETCOMPUTE_DELETE_METHOD(ws_node& operator=(ws_node const&));
            HETCOMPUTE_DELETE_METHOD(ws_node& operator=(ws_node&&));

        public:
            ws_node() : ws_node_base(), _left(nullptr), _right(nullptr), _tree(nullptr), _value() {}

            node_type* get_left(hetcompute::mem_order order = hetcompute::mem_order_seq_cst) const { return _left.load(order); }
            node_type* get_right(hetcompute::mem_order order = hetcompute::mem_order_seq_cst) const { return _right.load(order); }

            void set_left(node_type_pointer node, hetcompute::mem_order order = hetcompute::mem_order_seq_cst) { _left.store(node, order); }
            void set_right(node_type_pointer node, hetcompute::mem_order order = hetcompute::mem_order_seq_cst) { _right.store(node, order); }

            // Set rightmost bit for tree pointer to 0
            // Used to check completion of a leaf node.
            tree_type_pointer get_tree() const
            {
                return reinterpret_cast<tree_type_pointer>(reinterpret_cast<intptr_t>(_tree) & (~NODE_COMPLETE_BIT));
            }

            void set_completed() { _tree = reinterpret_cast<tree_type_pointer>(reinterpret_cast<intptr_t>(_tree) | NODE_COMPLETE_BIT); }

            bool is_completed() { return (reinterpret_cast<intptr_t>(_tree) & NODE_COMPLETE_BIT) == NODE_COMPLETE_BIT; }

            // getter and setters
            inline const T& peek_value() const { return _value; }
            inline T&       peek_value() { return _value; }
            inline void     set_value(T& val) { _value = val; }
            inline void     set_value(T&& val) { _value = std::move(val); }

            // every node in work steal tree must have two children
            // so only checking left child is sufficient.
            bool is_leaf() { return get_left(hetcompute::mem_order_relaxed) == nullptr; }

            // Returns the last working index of a non-leaf node before stealing
            // For leaf node the function will return "last"
            // Useful for multi-pass algorithm such as pscan
            size_type get_split_idx() const { return this->get_left()->get_first() - 1; }

            // Attempts to claim ownership of a worksteal tree node.
            // One can only call this operation on a UNCLAIMED node.
            // Return true if the claim is successful and false otherwise.
            static bool try_own(node_type* n);

            // Attempts to steal from an OWNED worksteal tree node.
            // A node cannot be stolen if it's in state UNCLAIMED,
            // STOLEN (someone succeeds in stealing before you), or
            // the workload on the node was finished already. If
            // stealing is successful, the worker task is responsible
            // for expanding the node and work on the right child.
            static try_steal_result try_steal(node_type* n, const size_type blk_size, const T& init);

        }; // class ws_node<T>

        template <>
        class ws_tree<void>;

        // Specialization for ws_node class
        // Optimize for pfor_each opeartion in which intermediate results are unnessary
        template <>
        class ws_node<void> : public ws_node_base
        {
        public:
            typedef ws_node<void> node_type;
            typedef ws_tree<void> tree_type;

        private:
            typedef ws_node<void>*                 node_type_pointer;
            typedef ws_tree<void>*                 tree_type_pointer;
            typedef std::atomic<node_type_pointer> atomic_node_type_pointer;

            atomic_node_type_pointer _left;
            atomic_node_type_pointer _right;
            tree_type_pointer        _tree;

            ws_node(size_type first, size_type last, size_type progress, tree_type_pointer tree)
                : ws_node_base(first, last, progress), _left(nullptr), _right(nullptr), _tree(tree)
            {
            }

            friend class ws_tree<void>;
            friend class testing::pool_tests;

            HETCOMPUTE_DELETE_METHOD(ws_node(ws_node const&));
            HETCOMPUTE_DELETE_METHOD(ws_node(ws_node&&));
            HETCOMPUTE_DELETE_METHOD(ws_node& operator=(ws_node const&));
            HETCOMPUTE_DELETE_METHOD(ws_node& operator=(ws_node&&));

        public:
            ws_node() : ws_node_base(), _left(nullptr), _right(nullptr), _tree(nullptr) {}

            node_type* get_left(hetcompute::mem_order order = hetcompute::mem_order_seq_cst) const { return _left.load(order); }
            node_type* get_right(hetcompute::mem_order order = hetcompute::mem_order_seq_cst) const { return _right.load(order); }

            void set_left(node_type_pointer node, hetcompute::mem_order order = hetcompute::mem_order_seq_cst) { _left.store(node, order); }
            void set_right(node_type_pointer node, hetcompute::mem_order order = hetcompute::mem_order_seq_cst) { _right.store(node, order); }

            tree_type_pointer get_tree() const
            {
                return reinterpret_cast<tree_type_pointer>(reinterpret_cast<intptr_t>(_tree) & (~NODE_COMPLETE_BIT));
            }

            void set_completed() { _tree = reinterpret_cast<tree_type_pointer>(reinterpret_cast<intptr_t>(_tree) | NODE_COMPLETE_BIT); }

            bool is_completed() { return (reinterpret_cast<intptr_t>(_tree) & NODE_COMPLETE_BIT) == NODE_COMPLETE_BIT; }

            bool is_leaf() { return get_left(hetcompute::mem_order_relaxed) == nullptr; }

            static bool try_own(node_type* n);

            static try_steal_result try_steal(node_type* n, const size_type blk_size);

        }; // class ws_node<void>

        /**
          A worksteal tree is a binary tree representing how pfor workload
          is adaptively splitted amongst tasks. All the tree nodes are stored
          in a pre-allocated lock-free node pool defined in class node_pool.
        */
        template <typename T>
        class ws_tree : public ws_tree_base
        {
        public:
            typedef ws_node<T> node_type;
            // we use 256 here because the average number of tree
            // node for pfor micro-benchmarks is around 150. We
            // would like to use a power of 2 number and don't want
            // to create an extra slab in most cases.
            typedef node_pool<node_type, 256> pool_type;

            ws_tree(size_t first, size_t last, size_t blk_size, const T& identity, size_t doc = internal::num_execution_contexts())
                : ws_tree_base(blk_size, doc),
                  _node_pool(_max_tasks),
                  // we initialize progress to first+blk_size for root
                  // because root must be responsible for [first, first + blk_size).
                  // Otherwise checking for ALREADY_STOLEN in try_steal
                  // might fail.
                  _root(new (get_new_node_placement()) node_type(first, last, first + blk_size, this, identity))
            {
            }

            node_type* get_root() const { return _root; }
            size_type  range_start() const { return _root->get_first(); }

            // When we statically split workload and assign them to tasks in advance.
            static node_type* create_claimed_node(size_type first, size_type last, node_type* parent, const T& identity)
            {
                HETCOMPUTE_INTERNAL_ASSERT(parent != nullptr, "Invalid parent pointer.");

                ws_tree<T>* tree = parent->get_tree();
                HETCOMPUTE_INTERNAL_ASSERT(tree != nullptr, "Invalid tree pointer.");
                auto* placement = tree->get_new_node_placement();
                return new (placement) node_type(first, last, first, tree, identity);
            }

            // When we steal a node and start splitting, the left child
            // becomes uncaimed.
            static node_type* create_unclaimed_node(size_type first, size_type last, node_type* parent, const T& identity)
            {
                HETCOMPUTE_INTERNAL_ASSERT(parent != nullptr, "Invalid parent pointer.");

                ws_tree<T>* tree = parent->get_tree();
                HETCOMPUTE_INTERNAL_ASSERT(tree != nullptr, "Invalid tree pointer.");
                auto* placement = tree->get_new_node_placement();
                return new (placement) node_type(first, last, node_type::UNCLAIMED, tree, identity);
            }

            // When we steal a node and start splitting, the right child
            // becomes stolen
            static node_type* create_stolen_node(size_type first, size_type last, node_type* parent, const T& identity)
            {
                HETCOMPUTE_INTERNAL_ASSERT(parent != nullptr, "Invalid parent pointer.");
                HETCOMPUTE_INTERNAL_ASSERT(first != node_type::UNCLAIMED, "Stolen node can't be unclaimed");

                // This is part of the speculative tree creation algorithm.
                // when a stealer successfully steals a right node
                // to work on, encourages future search to go for
                // the left subtree to "own" the leftover work
                parent->inc_right_traversal();

                ws_tree<T>* tree = parent->get_tree();
                HETCOMPUTE_INTERNAL_ASSERT(tree != nullptr, "Invalid tree pointer.");
                auto* placement = tree->get_new_node_placement();
                return new (placement) node_type(first, last, first, tree, identity);
            }

            // Statically split work on a pre-built tree.
            // Assuming the number of execution contexts is P, we create
            // the first L = ceil{log2(P)} levels of tree and assign the leaf
            // nodes to the first 2^{L} tasks. These new nodes are placed
            // inside the node pool as usual.
            void split_tree_before_stealing(size_type nctx, const T& init);

            // overload for pscan
            void split_tree_before_stealing(size_type nctx);

            // if split tree before stealing, obtain the pre-assigned
            // leaf and return
            node_type* find_work_prealloc(size_type tid);

            // Find work on tree rooted at n
            // When recursively seaching for work from n, we choose
            // left or right based on speculation, i.e., based on historical
            // record of direction selection outcome, the search will
            // choose the subtree with possiblly less nodes.
            // The key assumption is that the event of stealing or owning is
            // highly correlated to previous direction selection.
            node_type* find_work_intree(node_type* n, size_type blk_size, const T& init);
            // overload for pscan
            node_type* find_work_intree(size_type tid, node_type* n, size_type blk_size);

        private:
            pool_type  _node_pool;
            node_type* _root;

            node_type* get_new_node_placement() { return _node_pool.get_next(); }

            // Disable all copying and movement.
            HETCOMPUTE_DELETE_METHOD(ws_tree(ws_tree const&));
            HETCOMPUTE_DELETE_METHOD(ws_tree(ws_tree&&));
            HETCOMPUTE_DELETE_METHOD(ws_tree& operator=(ws_tree const&));
            HETCOMPUTE_DELETE_METHOD(ws_tree& operator=(ws_tree&&));
        }; // class ws_tree<T>

        // Specification of ws_tree optimized for pfor_each
        template <>
        class ws_tree<void> : public ws_tree_base
        {
        public:
            typedef ws_node<void>             node_type;
            typedef node_pool<node_type, 256> pool_type;

            ws_tree(size_t first, size_t last, size_t blk_size, size_t stride, size_t doc = internal::num_execution_contexts())
                : ws_tree_base(blk_size, doc),
                  _node_pool(_max_tasks),
                  _root(new (get_new_node_placement()) node_type(first, last, first + blk_size, this)),
                  _stride(stride)
            {
            }

            node_type* get_root() const { return _root; }
            size_type  range_start() const { return _root->get_first(); }
            size_type  get_stride() const { return _stride; }

            static node_type* create_claimed_node(size_type first, size_type last, node_type* parent)
            {
                HETCOMPUTE_INTERNAL_ASSERT(parent != nullptr, "Invalid parent pointer.");

                ws_tree<void>* tree = parent->get_tree();
                HETCOMPUTE_INTERNAL_ASSERT(tree != nullptr, "Invalid tree pointer.");
                auto* placement = tree->get_new_node_placement();
                return new (placement) node_type(first, last, first, tree);
            }

            static node_type* create_unclaimed_node(size_type first, size_type last, node_type* parent)
            {
                HETCOMPUTE_INTERNAL_ASSERT(parent != nullptr, "Invalid parent pointer.");

                ws_tree<void>* tree = parent->get_tree();
                HETCOMPUTE_INTERNAL_ASSERT(tree != nullptr, "Invalid tree pointer.");
                auto* placement = tree->get_new_node_placement();
                return new (placement) node_type(first, last, node_type::UNCLAIMED, tree);
            }

            static node_type* create_stolen_node(size_type first, size_type last, node_type* parent)
            {
                HETCOMPUTE_INTERNAL_ASSERT(parent != nullptr, "Invalid parent pointer.");
                HETCOMPUTE_INTERNAL_ASSERT(first != node_type::UNCLAIMED, "Stolen node can't be unclaimed");

                parent->inc_right_traversal();

                ws_tree<void>* tree = parent->get_tree();
                HETCOMPUTE_INTERNAL_ASSERT(tree != nullptr, "Invalid tree pointer.");
                auto* placement = tree->get_new_node_placement();
                return new (placement) node_type(first, last, first, tree);
            }

            void split_tree_before_stealing(size_type nctx);

            node_type* find_work_prealloc(size_type tid);

            node_type* find_work_intree(node_type* n, size_type blk_size);

        private:
            pool_type  _node_pool;
            node_type* _root;
            // stride field specialized to pfor_each
            size_type _stride;

            node_type* get_new_node_placement() { return _node_pool.get_next(); }

            HETCOMPUTE_DELETE_METHOD(ws_tree(ws_tree const&));
            HETCOMPUTE_DELETE_METHOD(ws_tree(ws_tree&&));
            HETCOMPUTE_DELETE_METHOD(ws_tree& operator=(ws_tree const&));
            HETCOMPUTE_DELETE_METHOD(ws_tree& operator=(ws_tree&&));
        }; // class ws_tree<void>

#include <hetcompute/internal/patterns/workstealtree/wstree_impl.hh>

    }; // namespace internal
};     // namspace hetcompute
