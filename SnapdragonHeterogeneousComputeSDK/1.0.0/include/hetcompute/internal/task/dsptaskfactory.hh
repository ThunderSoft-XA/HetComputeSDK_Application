#pragma once

#include <hetcompute/internal/runtime-internal.hh>
#include <hetcompute/internal/task/dsptask.hh>
#include <hetcompute/internal/task/dsptraits.hh>
#include <hetcompute/internal/task/task.hh>
#include <hetcompute/internal/util/macros.hh>

#if !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
#include <hetcompute/internal/memalloc/concurrentbumppool.hh>
#include <hetcompute/internal/memalloc/taskallocator.hh>
#endif // !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)

namespace hetcompute
{
    template <typename Fn>
    class dsp_kernel;

    namespace internal
    {
        class group;

#if defined(HETCOMPUTE_HAVE_QTI_DSP)
        // Similar class than cputask_factory but for dsp tasks
        // TODO think how to combine functionality between the both
        //
        struct dsptask_factory
        {
            template <typename RawType, typename DspKernel, typename... Args>
            static RawType* create_task(group* g, legacy::task_attrs attrs, DspKernel&& hk, Args&&... args)
            {
                attrs = legacy::add_attr(attrs, attr::api20);

                // all dsp tasks are bounded at creation time
                auto initial_state = task::get_initial_state_bound();

                return create_task_impl<RawType, DspKernel, Args...>(initial_state,
                                                                         g,
                                                                         attrs,
                                                                         std::forward<DspKernel>(hk),
                                                                         std::forward<Args>(args)...);
            }

            template <typename RawType, typename DspKernel, typename... Args>
            static RawType* create_anonymous_task(group* g, legacy::task_attrs attrs, DspKernel&& hk, Args&&... args)
            {
                attrs = legacy::add_attr(attrs, attr::api20);
                return create_task_impl<RawType, DspKernel, Args...>(task::get_initial_state_anonymous(),
                                                                         g,
                                                                         attrs,
                                                                         std::forward<DspKernel>(hk),
                                                                         std::forward<Args>(args)...);
            }

        private:
            template <typename RawType, typename DspKernel, typename... Args>
            static RawType*
            create_task_impl(task::state_snapshot initial_state, group* g, legacy::task_attrs attrs, DspKernel&& hk, Args&&... args)
            {
                return new dsptask<DspKernel, Args...>(initial_state,
                                                               g,
                                                               attrs,
                                                               std::forward<DspKernel>(hk),
                                                               std::forward<Args>(args)...);
            }
        };

        template <typename Fn>
        struct task_factory_dispatch<::hetcompute::dsp_kernel<Fn>> : public dsptask_factory
        {
        private:
            using kernel  = typename ::hetcompute::dsp_kernel<Fn>;
            using fn_type = typename kernel::fn_type;

        protected:
            using collapsed_task_type     = typename hetcompute::task_ptr<void(void)>; // typename kernel::collapsed_task_type;
            using non_collapsed_task_type = typename hetcompute::task_ptr<void(void)>; // typename kernel::non_collapsed_task_type;

            using collapsed_raw_task_type     = hetcompute::internal::task; // typename kernel::collapsed_raw_task_type;
            using non_collapsed_raw_task_type = hetcompute::internal::task; // typename kernel::non_collapsed_raw_task_type;

            // returns an rvalue reference to the function within k
            static fn_type&& get_fn(kernel&& k) { return static_cast<fn_type&&>(k.get_fn()); }

            // returns an lvalue reference to the function within k
            static fn_type& get_fn(kernel& k) { return static_cast<fn_type&>(k.get_fn()); }

            template <typename DspKernel, typename... TaskArgs>
            static collapsed_raw_task_type* create_collapsed_task(DspKernel&& k, TaskArgs&&... args)
            {
                // Check that the arguments match the dsp function parameters
                // The check is done at compilation time and has no overhead because the object is immediately
                // destroyed after creation
                hetcompute::internal::check<typename std::remove_reference<DspKernel>::type, TaskArgs...>();

                auto t = create_task<collapsed_raw_task_type, DspKernel, TaskArgs...>(nullptr,
                                                                                          k.get_attrs(),
                                                                                          std::forward<DspKernel>(k),
                                                                                          std::forward<TaskArgs>(args)...);

                return t;
            }

            template <typename DspKernel, typename... TaskArgs>
            static non_collapsed_raw_task_type* create_non_collapsed_task(DspKernel&& k, TaskArgs&&... args)
            {
                // Check that the arguments match the dsp function parameters
                // The check is done at compilation time and has no overhead
                hetcompute::internal::check<typename std::remove_reference<DspKernel>::type, TaskArgs...>();

                auto t = create_task<non_collapsed_raw_task_type, DspKernel, TaskArgs...>(nullptr,
                                                                                              k.get_attrs(),
                                                                                              std::forward<DspKernel>(k),
                                                                                              std::forward<TaskArgs>(args)...);
                return t;
            }

            template <typename DspKernel, typename... TaskArgs>
            static bool launch(bool notify, group* g, DspKernel&& k, TaskArgs&&... args)
            {
                // Check that the arguments match the dsp function parameters
                // The check is done at compilation time and has no overhead
                hetcompute::internal::check<typename std::remove_reference<DspKernel>::type, TaskArgs...>();

                auto attrs = k.get_attrs();
                // ensure the kernel does not have blocking attributes
                HETCOMPUTE_INTERNAL_ASSERT(has_attr(attrs, ::hetcompute::internal::legacy::attr::blocking) == false,
                                         "Dsp task cannot be blocking");

                k.set_attr(attr::anonymous);

                task* t = create_anonymous_task<non_collapsed_raw_task_type>(g,
                                                                             k.get_attrs(),
                                                                             std::forward<DspKernel>(k),
                                                                             std::forward<TaskArgs>(args)...);
                if (t == nullptr)
                {
                    return false;
                }

                send_to_runtime(t, nullptr, notify);
                return true;
            }
        }; // task_factory_dispatch<dsp_kernel<T>>

#endif     // defined(HETCOMPUTE_HAVE_QTI_DSP)

    }; // namespace internal
};     // namespace hetcompute
