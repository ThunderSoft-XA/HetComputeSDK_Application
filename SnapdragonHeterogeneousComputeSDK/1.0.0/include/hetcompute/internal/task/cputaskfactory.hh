#pragma once

#include <hetcompute/internal/runtime-internal.hh>
#include <hetcompute/internal/task/cpukernel.hh>
#include <hetcompute/internal/task/cputask.hh>
#include <hetcompute/internal/task/task.hh>
#include <hetcompute/internal/util/macros.hh>
#include <hetcompute/internal/util/templatemagic.hh>

#if !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
#include <hetcompute/internal/memalloc/concurrentbumppool.hh>
#include <hetcompute/internal/memalloc/taskallocator.hh>
#endif // !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)

namespace hetcompute
{
    template<typename Fn> class cpu_kernel;

    namespace internal
    {
        class group;

        // This data structure is in charge of creating cputasks.  Once we
        // clean up the old APIs, this should be the only way to create
        // cputasks, and will become a friend of
        // cputask<TypeInfo>
        struct cputask_factory
        {
            template <typename traits, typename RawType, typename TypeInfo, typename Fn, typename... Args>
            static RawType* create_task(group* g, legacy::task_attrs attrs, Fn&& fn, Args&&... args)
            {
                attrs = legacy::add_attr(attrs, attr::api20);
                auto initial_state =
                    (traits::arity::value == sizeof...(Args)) ? task::get_initial_state_bound() : task::get_initial_state_unbound();

                return create_task_impl<RawType, TypeInfo, Fn>(initial_state, g, attrs, std::forward<Fn>(fn), std::forward<Args>(args)...);
            }

            template <typename RawType, typename TaskTypeInfo, typename Fn, typename... Args>
            static RawType* create_anonymous_task(group* g, legacy::task_attrs attrs, Fn&& fn, Args&&... args)
            {
                attrs = legacy::add_attr(attrs, attr::api20);
                return create_task_impl<RawType, TaskTypeInfo, Fn>(task::get_initial_state_anonymous(),
                                                                   g,
                                                                   attrs,
                                                                   std::forward<Fn>(fn),
                                                                   std::forward<Args>(args)...);
            }

            template <typename ReturnType, typename... FnArgs>
            static value_cputask<ReturnType>* create_value_task(FnArgs&&... args)
            {
                static auto s_default_cpu_attr = legacy::create_task_attrs(task_attr_cpu(), attr::api20);

#if !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
                char* task_buffer = task_allocator::allocate(sizeof(value_cputask<ReturnType>));
                return new (task_buffer)
                    value_cputask<ReturnType>(task::get_initial_state_value_task(), nullptr, s_default_cpu_attr, std::forward<FnArgs>(args)...);
#else
                return new value_cputask<ReturnType>(task::get_initial_state_value_task(),
                                                     nullptr,
                                                     s_default_cpu_attr,
                                                     std::forward<FnArgs>(args)...);
#endif // !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
            }

        private:
            template <typename RawType, typename TaskTypeInfo, typename Fn, typename... Args>
            static RawType* create_task_impl(task::state_snapshot initial_state, group* g, legacy::task_attrs attrs, Fn&& fn, Args&&... args)
            {
#if !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
                char* buf = task_allocator::allocate(sizeof(cputask<TaskTypeInfo>));
                return new (buf) cputask<TaskTypeInfo>(initial_state, g, attrs, std::forward<Fn>(fn), std::forward<Args>(args)...);
#else
                return new cputask<TaskTypeInfo>(initial_state, g, attrs, std::forward<Fn>(fn), std::forward<Args>(args)...);
#endif
            }
        };  // struct cputask_factory

        // Helper struct for task_factory_dispatch<Fn> (i.e. non cpu_kernel case):
        //  - ConvertToRegular == true ==> launches user-function as a regular task
        //  - o.w., does nothing
        template <bool ConvertToRegular, typename RawType, typename TypeInfo, typename UserFn, typename... Args>
        struct launch_anonymous_as_regular_task;

        template <typename RawType, typename TypeInfo, typename UserFn, typename... Args>
        struct launch_anonymous_as_regular_task<true, RawType, TypeInfo, UserFn, Args...> : public cputask_factory
        {
            static bool launch(group* g, UserFn&& fn, Args&&... args)
            {
                using CpuKernel = hetcompute::internal::cpu_kernel<UserFn>;
                using traits    = typename CpuKernel::user_code_traits;
                auto  k         = hetcompute::internal::cpu_kernel<UserFn>(fn);
                task* t =
                    create_task<traits, RawType, TypeInfo>(g, k.get_attrs(), std::forward<UserFn>(k.get_fn()), std::forward<Args>(args)...);
                if (t == nullptr)
                    return false;

                t->launch(nullptr, nullptr);
                return true;
            }
        };

        template <typename RawType, typename TypeInfo, typename UserFn, typename... Args>
        struct launch_anonymous_as_regular_task<false, RawType, TypeInfo, UserFn, Args...>
        {
            static bool launch(group*, UserFn&&, Args&&...)
            {
                // do nothing
                return true;
            }
        };

        template <typename Fn, class Enable = void>
        struct task_factory_dispatch;

        template <typename Fn>
        struct task_factory_dispatch<Fn> : public cputask_factory
        {
        private:
            using kernel = typename ::hetcompute::cpu_kernel<Fn>;
            using traits = typename kernel::user_code_traits;

            using collapsed_task_type_info     = typename kernel::collapsed_task_type_info;
            using non_collapsed_task_type_info = typename kernel::non_collapsed_task_type_info;

        protected:
            using collapsed_task_type     = typename kernel::collapsed_task_type;
            using non_collapsed_task_type = typename kernel::non_collapsed_task_type;

            using collapsed_raw_task_type     = typename kernel::collapsed_raw_task_type;
            using non_collapsed_raw_task_type = typename kernel::non_collapsed_raw_task_type;

            template <typename UserFn, typename... Args>
            static collapsed_raw_task_type* create_collapsed_task(UserFn&& fn, Args&&... args)
            {
                auto default_cpu_attr = legacy::create_task_attrs(task_attr_cpu());
                return create_task<traits, collapsed_raw_task_type, collapsed_task_type_info>(nullptr,
                                                                                              default_cpu_attr,
                                                                                              std::forward<UserFn>(fn),
                                                                                              std::forward<Args>(args)...);
            }

            template <typename UserFn, typename... Args>
            static non_collapsed_raw_task_type* create_non_collapsed_task(UserFn&& fn, Args&&... args)
            {
                auto default_cpu_attr = legacy::create_task_attrs(task_attr_cpu());
                return create_task<traits, non_collapsed_raw_task_type, non_collapsed_task_type_info>(nullptr,
                                                                                                      default_cpu_attr,
                                                                                                      std::forward<UserFn>(fn),
                                                                                                      std::forward<Args>(args)...);
            }

            // returns an rvalue reference to the function within k
            template <typename fn_type, typename kernel>
            static fn_type&& get_fn(kernel&& k)
            {
                return static_cast<fn_type&&>(k.get_fn());
            }

            // returns an lvalue reference to the function within k
            template <typename fn_type, typename kernel>
            static fn_type& get_fn(kernel& k)
            {
                return static_cast<fn_type&>(k.get_fn());
            }

            template <typename UserFn, typename... Args>
            static bool launch(bool notify, group* g, UserFn&& fn, Args&&... args)
            {
                // Anonymous tasks created from functors, lambdas or function
                // pointers cannot be set as blocking.  This is because that
                // setting them as blocking requires using a cpu_kernel, we can just
                // create a task with attributes cpu and anonymous and send it to
                // the runtime immediately.
                //
                // Additionally, check if the task has any buffer arguments. Launch as
                // regular task instead of anonymous, as the presence of buffer arguments
                // may dynamically set up predecessor/successor dependences to this task
                // on a buffer conflict.
                constexpr size_t num_buffer_args = num_buffer_ptrs_in_tuple<std::tuple<Args...>>::value;
                if (num_buffer_args == 0)
                {
                    auto default_anonymous_cpu_attr = legacy::create_task_attrs(task_attr_cpu(), attr::anonymous);
                    auto t                          = create_anonymous_task<non_collapsed_raw_task_type, non_collapsed_task_type_info>(g,
                                                                                                              default_anonymous_cpu_attr,
                                                                                                              std::forward<UserFn>(fn),
                                                                                                              std::forward<Args>(args)...);
                    if (t == nullptr)
                        return false;

                    send_to_runtime(t, nullptr, notify);
                    return true;
                }

                return launch_anonymous_as_regular_task<(num_buffer_args > 0),
                                                        non_collapsed_raw_task_type,
                                                        non_collapsed_task_type_info,
                                                        UserFn,
                                                        Args...>::launch(g, std::forward<UserFn>(fn), std::forward<Args>(args)...);
            }

        };  // struct task_factory_dispatch

        template <typename Fn>
        struct task_factory_dispatch<::hetcompute::cpu_kernel<Fn>> : public cputask_factory
        {
        private:
            using kernel = typename ::hetcompute::cpu_kernel<Fn>;
            using traits = typename kernel::user_code_traits;

            using collapsed_task_type_info     = typename kernel::collapsed_task_type_info;
            using non_collapsed_task_type_info = typename kernel::non_collapsed_task_type_info;
            using fn_type                      = typename kernel::user_code_type_in_task;

        protected:
            using collapsed_task_type     = typename kernel::collapsed_task_type;
            using non_collapsed_task_type = typename kernel::non_collapsed_task_type;

            using collapsed_raw_task_type     = typename kernel::collapsed_raw_task_type;
            using non_collapsed_raw_task_type = typename kernel::non_collapsed_raw_task_type;

            // returns an rvalue reference to the function within k
            static fn_type&& get_fn(kernel&& k) { return static_cast<fn_type&&>(k.get_fn()); }

            // returns an lvalue reference to the function within k
            static fn_type& get_fn(kernel& k) { return static_cast<fn_type&>(k.get_fn()); }

            template <typename CpuKernel, typename... Args>
            static collapsed_raw_task_type* create_collapsed_task(CpuKernel&& k, Args&&... args)
            {
                return create_task<traits, collapsed_raw_task_type, collapsed_task_type_info>(nullptr,
                                                                                              k.get_attrs(),
                                                                                              get_fn(std::forward<CpuKernel>(k)),
                                                                                              std::forward<Args>(args)...);
            }

            template <typename CpuKernel, typename... Args>
            static non_collapsed_raw_task_type* create_non_collapsed_task(CpuKernel&& k, Args&&... args)
            {
                return create_task<traits, non_collapsed_raw_task_type, non_collapsed_task_type_info>(nullptr,
                                                                                                      k.get_attrs(),
                                                                                                      get_fn(std::forward<CpuKernel>(k)),
                                                                                                      std::forward<Args>(args)...);
            }

            template <typename CpuKernel, typename... Args>
            static bool launch(bool notify, group* g, CpuKernel&& k, Args&&... args)
            {
                // Anonymous tasks created from bodies might have been marked
                // as blocking. Or, the anonymous task may have buffer arguments,
                // which on buffer conflicts could lead to the dynamic addition of
                // predecessor or successor task dependences, something not checked
                // for by anonymous tasks. In either case, we shouldn't mark the task
                // as anonymous and launch them using the standard, non-optimized
                // launch procedure. Otherwise we launch them using the optimized
                // path.
                auto attrs = k.get_attrs();

                using stripped_CpuKernel           = typename strip_ref_cv<CpuKernel>::type;
                constexpr size_t num_buffer_params = num_buffer_ptrs_in_tuple<typename stripped_CpuKernel::args_tuple>::value;

                if (num_buffer_params == 0 &&
                    has_attr(attrs, ::hetcompute::internal::legacy::attr::blocking) == false)
                {
                    k.set_attr(attr::anonymous);
                    task* t =
                        create_anonymous_task<non_collapsed_raw_task_type, non_collapsed_task_type_info>(g,
                                                                                                         k.get_attrs(),
                                                                                                         get_fn(std::forward<CpuKernel>(k)),
                                                                                                         std::forward<Args>(args)...);
                    if (t == nullptr)
                    {
                        return false;
                    }

                    send_to_runtime(t, nullptr, notify);
                    return true;
                }

                //
                task* t = create_task<traits, non_collapsed_raw_task_type, non_collapsed_task_type_info>(g,
                                                                                                         k.get_attrs(),
                                                                                                         get_fn(std::forward<CpuKernel>(k)),
                                                                                                         std::forward<Args>(args)...);
                if (t == nullptr)
                {
                    return false;
                }

                t->launch(nullptr, nullptr);
                return true;
            }
        }; // task_factory_dispatch<cpu_kernel<T>>

    };  // namespace internal
};  // namespace hetcompute
