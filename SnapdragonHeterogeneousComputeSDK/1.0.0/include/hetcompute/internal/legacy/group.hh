#pragma once

#include <string>

#include <hetcompute/internal/task/group_shared_ptr.hh>
#include <hetcompute/internal/task/task_shared_ptr.hh>
#include <hetcompute/internal/legacy/types.hh>
#include <hetcompute/hetcompute_error.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace legacy
        {
            /**
             * Legacy APIs, used by external headers.
             */

            group_shared_ptr create_group(std::string const& name);

            group_shared_ptr create_group(const char* name);

            group_shared_ptr create_group();

            group_shared_ptr intersect(group_shared_ptr const& a, group_shared_ptr const& b);

            void join_group(group_shared_ptr const& g, task_shared_ptr const& t);

            hc_error wait_for(group_shared_ptr const& g);

            void finish_after(group_shared_ptr const& g);

            void spin_wait_for(group_shared_ptr const& group);

            void cancel(group_shared_ptr const& group);

            bool canceled(group_shared_ptr const& group);

            group_shared_ptr operator&(group_shared_ptr const& a, group_shared_ptr const& b);

        }; // namespace legacy

    }; // namespace internal

}; // namespace hetcompute
