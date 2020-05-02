#pragma once

#include <mutex>
#include <set>
#include <hetcompute/internal/task/group_signature.hh>
#include <hetcompute/internal/task/group_shared_ptr.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace group_misc
        {
            enum lattice_lock_policy
            {
                acquire_lattice_lock,
                do_not_acquire_lattice_lock
            };

            /// This class implements the needed algorithms for inserting a new meet group
            /// into the group lattice.
            class lattice
            {
            public:
                // We use a type to try different locks for lattice operations
                typedef std::recursive_mutex lock_type;

            private:
                // Type used internally by the insertion algorithms
                typedef std::set<group*> group_set;
                static lock_type         s_mutex;

            public:
                /// Check the lattice for the intersection of a and b and if using
                /// their bitmaps and if it does not exist it creates a new meet
                /// and add it to the lattice and update the right meet db.
                /// It also moves the tasks from a possible donor group to new created one.
                ///
                /// @param a, first group to be intersected
                /// @param b, first group to be intersected
                /// @param current_group, the donor group that
                /// its tasks will move to the intersection group
                /// @return the new or found meet node
                static group_shared_ptr create_meet_node(group* a, group* b, group* current_group = nullptr);

                static lock_type& get_mutex() { return s_mutex;}

            private:
                /// Similar to find_sup but for two ancestors
                ///
                /// @param new_b, the new bitmap (of the intersection group)
                /// @param ancestor1, the first group to be searched
                /// @param ancestor2, the second group to be searched
                /// @param sup, the list of superiors collected so far
                static void find_sup(group_signature& new_b, group* ancestor1, group* ancestor2, group_set& sup)
                {
                    find_sup(new_b, ancestor1, sup);
                    find_sup(new_b, ancestor2, sup);
                }

                /// Similar to find_inf but for two ancestors
                ///
                /// @param new_b, the new bitmap (of the intersection group)
                /// @param ancestor1, the first group to be searched
                /// @param ancestor2, the second group to be searched
                /// @param sup, the list of superiors collected so far
                static void find_inf(group_signature& new_b, group* ancestor1, group* ancestor2, group_set& inf)
                {
                    find_inf(new_b, ancestor1, inf);
                    find_inf(new_b, ancestor2, inf);
                }

                /// Looks at all children of the ancestor
                /// find all the ones (superior) that are subset of the new bitmap
                ///
                /// @param new_b, the new bitmap (of the intersection group)
                /// @param ancestor, the group to be checked against
                /// @param sup, the list of superiors collected so far
                static void find_sup(group_signature& new_b, group* ancestor, group_set& sup);

                /// Similar to find_sup but it is called recursively on the children of
                /// ancestor input
                ///
                /// @param new_b, the new bitmap (of the intersection group)
                /// @param ancestor, the group to be checked against
                /// @param sup, the list of superiors collected so far
                /// @param[out] found, indicates whether is superior has been found
                /// in this sublattice
                static void find_sup_rec(group_signature& new_b, size_t order, group* ancestor, group_set& sup, bool& found);

                /// Looks at all children of the ancestor
                /// find all the ones (inferiors) that the new bitmap is a subset of
                ///
                /// @param new_b, the new bitmap (of the intersection group)
                /// @param ancestor, the group to be checked against
                /// @param inf, the list of inferiord collected so far
                static void find_inf(group_signature& new_b, group* ancestor, group_set& inf);

                /// Similar to find_inf but it is called recursively on the children of
                ///
                /// @param new_b, the new bitmap (of the intersection group)
                /// @param ancestor, the group to be checked against
                /// @param inf, the list of inferiord collected so far
                static void find_inf_rec(group_signature& new_b, size_t order, group* node, group_set& inf);

                /// Inserts the new intersection (meet) group into lattice
                ///
                /// @param new_group, the new created meet group
                /// @param sup, the set of superiors of the new group
                /// @param inf, the set of inferiors of the new group
                static void insert_in_lattice(group* new_group, group_set const& sup, group_set const& inf);

                /// Specialized version of insert_in_lattice when intersecting groups
                /// are childfree leaves. This skips the inf/sup computation
                ///
                /// @param new_group, the new created meet group
                /// @param a, the fist group
                /// @param b, the second group
                static void insert_in_lattice_childfree_leaves(group* new_group, group* a, group* b);

                /// Specialized version of insert_in_lattice when intersecting groups
                /// are childfree groups. This skips the inf computation
                ///
                /// @param new_group, the new created meet group
                /// @param sup, the set of superiors of the new group
                static void insert_in_lattice_childfree_parents(group* new_group, group_set const& sup);

                /// Specialized version of insert_in_lattice when intersecting groups
                /// are leaf groups. This skips the sub computation
                ///
                /// @param new_group, the new created meet group
                /// @param a, the fist group
                /// @param b, the second group
                /// @param inf, the set of inferiors of the new group
                static void insert_in_lattice_leaves(group* new_group, group* a, group* b, group_set const& inf);
            };  // class lattice

        }; // namespace group_misc
    }; // namespace internal
}; // namespace hetcompute
