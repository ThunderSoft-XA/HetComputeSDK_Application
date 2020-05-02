#pragma once

#include <hetcompute/internal/task/cputaskfactory.hh>
#include <hetcompute/internal/task/gputaskfactory.hh>
#include <hetcompute/internal/task/group.hh>
#include <hetcompute/internal/task/dsptaskfactory.hh>
#include <hetcompute/internal/util/macros.hh>

#if !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
#include <hetcompute/internal/memalloc/concurrentbumppool.hh>
#include <hetcompute/internal/memalloc/taskallocator.hh>
#endif // !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)

namespace hetcompute
{
    template <typename Fn>
    class cpu_kernel;

    template <typename Fn>
    class dsp_kernel;

    template<typename Fn, typename ...Args>
    task_ptr<void()> create_task(const pattern::pfor<Fn, void>& pf, Args&&... args);

    template<typename Fn, typename ...Args>
    task_ptr<void()> create_task(const pattern::ptransformer<Fn>& ptf, Args&&... args);

    template<typename BinaryFn, typename ...Args>
    task_ptr<void()> create_task(const pattern::pscan<BinaryFn>& ps, Args&&... args);

    template<typename Compare, typename ...Args>
    task_ptr<void()> create_task(const pattern::psorter<Compare>& p, Args&&... args);

    namespace internal
    {
        template <typename Problem, typename Fn1, typename Fn2, typename Fn3, typename Fn4>
        hetcompute::task_ptr<>
        pdivide_and_conquer_async(Fn1&& is_base, Fn2&& base, Fn3&& split, Fn4&& merge, Problem p);

        template <typename F>
        struct is_pdnc : std::integral_constant<bool, false>
        {
        };

        template <typename Fn1, typename Fn2, typename Fn3, typename Fn4>
        struct is_pdnc<::hetcompute::pattern::pdivide_and_conquerer<Fn1, Fn2, Fn3, Fn4>> : std::integral_constant<bool, true>
        {
        };

        /// Use this template to check whether a type can be
        /// used to create a task.
        template <typename F, class Enable = void>
        struct is_taskable : std::integral_constant<bool, false>
        {
        };

        template <typename F>
        struct is_taskable<F, typename std::enable_if<!std::is_same<typename function_traits<F>::kind, user_code_types::invalid>::value>::type>
            : std::integral_constant<bool, true>
        {
        };

        template <typename F>
        struct is_taskable<::hetcompute::cpu_kernel<F>, void> : std::integral_constant<bool, true>
        {
        };

        template <typename F>
        struct is_taskable<::hetcompute::dsp_kernel<F>, void> : std::integral_constant<bool, true>
        {
        };

        template <typename... KernelArgs>
        struct is_taskable<::hetcompute::gpu_kernel<KernelArgs...>, void> : std::integral_constant<bool, true>
        {
        };

        template <typename Code, typename IsPattern = std::false_type, class Enable = void>
        struct task_factory;

        template <typename Code>
        struct task_factory<Code, std::false_type, typename std::enable_if<is_taskable<typename std::remove_reference<Code>::type>::value>::type>
            : public task_factory_dispatch<typename std::remove_reference<Code>::type>
        {
        private:
            using parent = task_factory_dispatch<typename std::remove_reference<Code>::type>;

        public:
            using collapsed_task_type     = typename parent::collapsed_task_type;
            using non_collapsed_task_type = typename parent::non_collapsed_task_type;

            using collapsed_raw_task_type     = typename parent::collapsed_raw_task_type;
            using non_collapsed_raw_task_type = typename parent::non_collapsed_raw_task_type;

            template <typename UserCode, typename... Args>
            static collapsed_raw_task_type* create_collapsed_task(UserCode&& code, Args&&... args)
            {
                return parent::create_collapsed_task(std::forward<UserCode>(code), std::forward<Args>(args)...);
            };

            template <typename UserCode, typename... Args>
            static non_collapsed_raw_task_type* create_non_collapsed_task(UserCode&& code, Args&&... args)
            {
                return parent::create_non_collapsed_task(std::forward<UserCode>(code), std::forward<Args>(args)...);
            };

            template <typename UserCode, typename... Args>
            static void launch(bool notify, group* g, UserCode&& code, Args&&... args)
            {
                g->inc_task_counter();
                bool success = parent::launch(notify, g, std::forward<UserCode>(code), std::forward<Args>(args)...);
                // If for some reason the factory never launches the task into the group,
                // then we must decrease the task counter to avoid
                if (!success)
                {
                    g->dec_task_counter();
                }
            }
        };

        // The static launch function defined here is to launch taskable patterns.
        // Note that patterns with possible return values are not directly launchable.
        template <typename Code>
        struct task_factory<Code, std::true_type, typename std::enable_if<!is_pdnc<typename std::decay<Code>::type>::value>::type>
        {
            template <typename Pattern = Code, typename... Args>
            static void launch(bool notify, group* g, Pattern&& p, Args&&... args)
            {
                HETCOMPUTE_UNUSED(notify);

                auto t        = ::hetcompute::create_task(std::forward<Pattern>(p), std::forward<Args>(args)...);
                auto task_ptr = ::hetcompute::internal::get_cptr(t);

                task_ptr->launch(g, nullptr);
            }
        };

        // We need special treatment for pdnc because MSVC has a different perception of
        // argument matching compared with gcc and clang. MSVC does not match create_task
        // to the correct version defined for pdivide_and_conquer while gcc and clang do.
        // Therefore, we need to directly call pdivide_and_conquer_async to get rid of incorrect
        // route to create_task which treats "Problem pbl" as a task argument.
        template <typename Code>
        struct task_factory<Code, std::true_type, typename std::enable_if<is_pdnc<typename std::decay<Code>::type>::value>::type>
        {
            template <typename Pattern = Code, typename Problem>
            static void launch(bool notify, group* g, Pattern&& p, Problem pbl)
            {
                HETCOMPUTE_UNUSED(notify);

                auto t = ::hetcompute::internal::pdivide_and_conquer_async(p._is_base_fn, p._base_fn, p._split_fn, p._merge_fn, pbl);

                auto task_ptr = ::hetcompute::internal::get_cptr(t);
                task_ptr->launch(g, nullptr);
            }
        };

    }; // namespace internal
};     // namespace hetcompute
