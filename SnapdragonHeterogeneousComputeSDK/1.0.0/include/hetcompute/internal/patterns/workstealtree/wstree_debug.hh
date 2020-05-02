#pragma once

#include <hetcompute/internal/patterns/workstealtree/wstree.hh>

namespace hetcompute
{
    namespace internal
    {
#ifdef HETCOMPUTE_ADAPTIVE_PFOR_DEBUG

        template <typename T>
        void print_node(typename ws_node<T>::node_type* root, std::ofstream& f)
        {
            // color map for each task
            // TODO: color not defined for task id > 14
            std::map<size_t, std::string> m = { { 0, "dodgerblue" }, { 1, "firebrick1" },      { 2, "aquamarine" },  { 3, "azure2" },
                                                { 4, "bisque1" },    { 5, "green" },           { 6, "deeppink1" },   { 7, "blueviolet" },
                                                { 8, "brown1" },     { 9, "burlywood2" },      { 10, "cadetblue2" }, { 11, "chartreuse2" },
                                                { 12, "crimson" },   { 13, "darkgoldenrod1" }, { 14, "darkgreen" } };

            if (root == nullptr)
            {
                return;
            }

            if (root->get_left(hetcompute::mem_order_relaxed) == nullptr)
            {
                root->set_progress_save(root->get_last() - root->get_first() + 1);
            }

            // logging information for each node
            f << "\t\"" << root->get_worker_id() << ": " << root->get_first() << "-" << root->get_last() << "\\n"
              << "pg: " << root->get_progress_save() << " tv: " << root->count_traversal() << "\""
              << " [color=" << m[root->get_worker_id()] << ", style=filled];\n";

            print_node<T>(root->get_left(hetcompute::mem_order_relaxed), f);
            print_node<T>(root->get_right(hetcompute::mem_order_relaxed), f);
        }

        template <typename T>
        void print_edge(typename ws_node<T>::node_type* curr, std::ofstream& f)
        {
            if (curr == nullptr)
            {
                return;
            }

            if (curr->get_left(hetcompute::mem_order_relaxed) != nullptr)
            {
                f << "\t\"" << curr->get_worker_id() << ": " << curr->get_first() << "-" << curr->get_last() << "\\n"
                  << "pg: " << curr->get_progress_save() << " tv: " << curr->count_traversal() << "\""
                  << " -> \"" << curr->get_left(hetcompute::mem_order_relaxed)->get_worker_id() << ": "
                  << curr->get_left(hetcompute::mem_order_relaxed)->get_first() << "-"
                  << curr->get_left(hetcompute::mem_order_relaxed)->get_last() << "\\n"
                  << "pg: " << curr->get_left(hetcompute::mem_order_relaxed)->get_progress_save()
                  << " tv: " << curr->get_left(hetcompute::mem_order_relaxed)->count_traversal() << "\";\n";
            }

            if (curr->get_right(hetcompute::mem_order_relaxed) != nullptr)
            {
                f << "\t\"" << curr->get_worker_id() << ": " << curr->get_first() << "-" << curr->get_last() << "\\n"
                  << "pg: " << curr->get_progress_save() << " tv: " << curr->count_traversal() << "\""
                  << " -> \"" << curr->get_right(hetcompute::mem_order_relaxed)->get_worker_id() << ": "
                  << curr->get_right(hetcompute::mem_order_relaxed)->get_first() << "-"
                  << curr->get_right(hetcompute::mem_order_relaxed)->get_last() << "\\n"
                  << "pg: " << curr->get_right(hetcompute::mem_order_relaxed)->get_progress_save()
                  << " tv: " << curr->get_right(hetcompute::mem_order_relaxed)->count_traversal() << "\";\n";
            }

            print_edge<T>(curr->get_left(hetcompute::mem_order_relaxed), f);
            print_edge<T>(curr->get_right(hetcompute::mem_order_relaxed), f);
        }

        // Print out configuration and edge for graphviz
        // Print with level-order traversal
        template <typename T>
        void print_tree(const typename ws_node<T>::tree_type& tree)
        {
            // set output file to tree.dot
            std::ofstream treef("tree.dot");

            treef << "digraph ws_tree {\n";
            treef << "\tsize=\"6,6\";\n";
            treef << "//Start Node Section\n";
            print_node<T>(tree.get_root(), treef);
            treef << "//End Node Section\n";
            treef << "\n";
            print_edge<T>(tree.get_root(), treef);
            treef << "}\n";

            treef.close();
        }

#endif // HETCOMPUTE_ADAPTIVE_PFOR_DEBUG

    }; // namespace internal
};     // namspace hetcompute
