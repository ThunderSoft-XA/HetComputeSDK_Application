#pragma once

// First include because we're implementing it.
// We treat this file as a cc file that implements
// the template method in hetcompute::group

#include <hetcompute/group.hh>

// Include internal hetcompute headers
#include <hetcompute/internal/task/group.hh>
#include <hetcompute/internal/task/task.hh>
#include <hetcompute/internal/task/internal_taskfactory.hh>

namespace hetcompute
{
    namespace internal
    {
        inline ::hetcompute::internal::group* c_ptr(::hetcompute::group* g) { return g->get_raw_ptr(); }

        namespace group_launch
        {
            template <typename Code, typename... Args>
            struct launch_code
            {
                using decayed_type = typename std::decay<Code>::type;
                using is_pattern =
                    typename std::conditional<internal::is_pattern<decayed_type>::value, std::true_type, std::false_type>::type;

                static_assert(::hetcompute::internal::is_hetcompute_task_ptr<decayed_type>::value == false, "Invalid launch for Code");

                // We do not allow launch of certain patterns because due to non-void return values.
                static_assert(::hetcompute::internal::is_group_launchable<decayed_type>::value == true,
                              "Group launch is not supported due to non-void return value of the pattern.");

                template <typename CodeType = Code, typename T>
                static void launch_impl(bool notify, ::hetcompute::internal::group* gptr, CodeType&& code, T&&, Args&&... args)
                {
                    using factory = ::hetcompute::internal::task_factory<CodeType, is_pattern>;

                    factory::launch(notify, gptr, std::forward<CodeType>(code), std::forward<Args>(args)...);
                }
            };

            template <typename T, typename... Args>
            struct launch_task
            {
                using decayed_type = typename std::decay<T>::type;

                static_assert(::hetcompute::internal::is_hetcompute_task_ptr<decayed_type>::value, "Invalid launch for tasks");

                static void launch_impl(bool notify, ::hetcompute::internal::group* gptr, T&& t, std::true_type, Args&&... args)
                {
                    HETCOMPUTE_UNUSED(notify);

                    static_assert(is_hetcompute_task_ptr<decayed_type>::has_arguments == true,
                                  "Invalid number of arguments for launching a task by the group.");

                    auto ptr = ::hetcompute::internal::get_cptr(t);

                    HETCOMPUTE_INTERNAL_ASSERT(ptr != nullptr, "Unexpected null task<>.");

                    t->bind_all(std::forward<Args>(args)...);
                    ptr->launch(gptr, nullptr);
                }

                static void launch_impl(bool notify, ::hetcompute::internal::group* gptr, T&& t, std::false_type, Args&&...)
                {
                    HETCOMPUTE_UNUSED(notify);

                    static_assert(sizeof...(Args) == 0, "Invalid number of arguments for launching a task by the group.");

                    task* ptr = ::hetcompute::internal::get_cptr(t);
                    HETCOMPUTE_INTERNAL_ASSERT(ptr != nullptr, "Unexpected null task<>.");

                    ptr->launch(gptr, nullptr);
                }
            };

        }; // namespace hetcompute::internal::group_launch

        namespace group_intersect
        {
            inline internal::group_shared_ptr intersect_impl(internal::group* a, internal::group* b)
            {
                // If a group meets nullptr, the result is nullptr.
                if (a == nullptr || b == nullptr)
                {
                    return nullptr;
                }

                if (a->is_ancestor_of(b))
                {
                    return internal::group_shared_ptr(b);
                }

                if (b->is_ancestor_of(a))
                {
                    return internal::group_shared_ptr(a);
                }

                return internal::group_factory::merge_groups(a, b);
            }

        }; // namespace hetcompute::internal::group_intersect

    }; // namespace hetcompute::internal

    inline void group::add(task_ptr<> const& task)
    {
        HETCOMPUTE_INTERNAL_ASSERT(get_raw_ptr() != nullptr, "Unexpected null group.");
        auto t = internal::c_ptr(task);
        HETCOMPUTE_API_ASSERT(t, "null task pointer.");
        t->join_group(get_raw_ptr(), true);
    }

    template <typename TaskPtrOrCode, typename... Args>
    void group::launch(TaskPtrOrCode&& task_or_code, Args&&... args)
    {
        auto g = get_raw_ptr();
        HETCOMPUTE_INTERNAL_ASSERT(g != nullptr, "Unexpected null group.");

        using decayed_type = typename std::decay<TaskPtrOrCode>::type;

        // First we figure out whether TaskPtrOrCode is a task_ptr
        // or Code
        using launch_policy = typename std::conditional<internal::is_hetcompute_task_ptr<decayed_type>::value,
                                                        internal::group_launch::launch_task<TaskPtrOrCode, Args...>,
                                                        internal::group_launch::launch_code<TaskPtrOrCode, Args...>>::type;

        using has_args = typename std::conditional<sizeof...(Args) == 0, std::false_type, std::true_type>::type;

        launch_policy::launch_impl(true, g, std::forward<TaskPtrOrCode>(task_or_code), has_args(), std::forward<Args>(args)...);
    }

}; // namespace hetcompute
