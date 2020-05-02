#pragma once

#include <hetcompute/internal/legacy/attr.hh>
#include <hetcompute/internal/legacy/attrobjs.hh>
#include <hetcompute/internal/legacy/gpukernel.hh>

#include <hetcompute/internal/task/gputask.hh>
#include <hetcompute/internal/util/macros.hh>
#include <hetcompute/internal/runtime-internal.hh>
#include <hetcompute/internal/task/cputask.hh>
#include <hetcompute/internal/task/group.hh>
#include <hetcompute/internal/task/task.hh>
#include <hetcompute/internal/util/templatemagic.hh>

#if !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
#include <hetcompute/internal/memalloc/taskallocator.hh>
#endif // !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)

namespace hetcompute
{
    namespace internal
    {
        namespace legacy
        {
            template <typename Body>
            struct body_wrapper_base;

            /// Matches functions, lambdas, and member functions used
            /// to create tasks.
            template <typename Fn>
            struct body_wrapper_base
            {
                typedef function_traits<Fn> traits;

                /// Returns function, lambda or member function.
                /// @return Fn -- Function, lambda or member function used as body.
                HETCOMPUTE_CONSTEXPR Fn& get_fn() const { return _fn; }

                /// Returns default attributes because the user did not
                /// specify attributes for Fn.
                /// @return legacy::task_attrs Default attributes for body.
                HETCOMPUTE_CONSTEXPR legacy::task_attrs get_attrs() const { return legacy::create_task_attrs(); }

                /// Constructor
                /// @param fn Function
                HETCOMPUTE_CONSTEXPR explicit body_wrapper_base(Fn&& fn) : _fn(fn) {}

                /// Creates tasks from body.
                /// @return task*  Pointer to the newly created task.
                task* create_task()
                {
                    return create_task_internal(std::forward<Fn>(get_fn()), nullptr, legacy::create_task_attrs(hetcompute::internal::attr::none));
                }

                /// Creates tasks from body.
                /// @param g Group the task should be added to.
                /// @param attrs Tasks attributes.
                /// @return task*  Pointer to the newly created task.
                task* create_task(group* g, legacy::task_attrs attrs)
                {
                     return create_task_internal(std::forward<Fn>(get_fn()), g, attrs);
                }

                /// Creates tasks from fn.
                /// @param fn Code that the task will execute.
                /// @return task*  Pointer to the newly created task.
                static task* create_task(Fn&& fn)
                {
                    task_attrs t_attr = legacy::create_task_attrs(hetcompute::internal::attr::none);

                    // use internal implementation to avoide duplicate code
                    return create_task_internal(std::forward<Fn>(fn), nullptr, t_attr);
                }

                /// Creates tasks from fn and adds it to group.
                /// @param fn Code that the task will execute.
                /// @param g Group the task should be added to.
                /// @return task*  Pointer to the newly created task.
                static task* create_task(Fn&& fn, group* g)
                {
                    task_attrs t_attr = legacy::create_task_attrs(hetcompute::internal::attr::none);

                    // use internal implementation to avoide duplicate code
                    return create_task_internal(fn, g, t_attr);
                }

            private:
                Fn& _fn;

                /// Creates tasks from fn and the attributes and adds it to group.
                /// @param fn Code that the task will execute.
                /// @param g Group the task should be added to.
                /// @param t_attr attributes of the created task.
                /// @return task*  Pointer to the newly created task.
                static task* create_task_internal(Fn&& fn, group* g, legacy::task_attrs t_attr)
                {
                    typedef typename internal::task_type_info<Fn, true> task_type_info;
                    typedef typename task_type_info::user_code user_code;

#if !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
                    char* task_buffer = task_allocator::allocate(sizeof(cputask<task_type_info>));
                    auto  t           = new (task_buffer) cputask<task_type_info>(g, t_attr, std::forward<user_code>(fn));
#else
                    auto t = new cputask<task_type_info>(g, t_attr, std::forward<user_code>(fn));
#endif // !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)

                    return t;
                }
            }; // struct body_wrapper_base<Fn>

            /// Matches functions, lambdas, and member functions used
            /// to create tasks that are wrapped in legacy::body_with_attrs.
            template <typename Fn>
            struct body_wrapper_base<legacy::body_with_attrs<Fn>>
            {
                typedef function_traits<Fn> traits;

                /// Returns function, lambda or member function that was wrapped in legacy::body_with_attrs.
                /// @return Fn -- Function, lambda or member function used as body.
                HETCOMPUTE_CONSTEXPR Fn& get_fn() const { return _b.get_body(); }

                /// Returns attributes from legacy::body_with_attrs.
                /// @return task_attrs Attributes from legacy::body_with_attrs.
                HETCOMPUTE_CONSTEXPR legacy::task_attrs get_attrs() const { return _b.get_attrs(); }

                /// Constructor
                /// @param b legacy::body_with_attrs wrapper
                HETCOMPUTE_CONSTEXPR explicit body_wrapper_base(legacy::body_with_attrs<Fn>&& b) : _b(b) {}

                /// Creates tasks from body, using the attributes in with_attrs.
                /// @return task*  Pointer to the newly created task.
                task* create_task() { return create_task_internal(std::forward<Fn>(get_fn()), nullptr, get_attrs()); }

                /// Creates tasks from body.
                /// @param g Group the task should be added to.
                /// @param attrs Tasks attributes.
                /// @return task*  Pointer to the newly created task.
                task* create_task(group* g, legacy::task_attrs attrs) { return create_task_internal(std::forward<Fn>(get_fn()), g, attrs); }

                /// Creates tasks from legacy::body_with_attrs.
                /// @param attr_body Code and attributes for the task.
                /// @return task*  Pointer to the newly created task.
                static task* create_task(legacy::body_with_attrs<Fn>&& attrd_body)
                {
                    return create_task_internal(std::forward<legacy::body_with_attrs<Fn>>(attrd_body), nullptr, attrd_body.get_attrs());
                }

                /// Creates tasks from fn and adds it to group.
                /// @param fn Code and atrributes for the new task.
                /// @param g Group the task should be added to.
                /// @return task*  Pointer to the newly created task.
                static task* create_task(legacy::body_with_attrs<Fn>&& attrd_body, group* g)
                {
                    return create_task_internal(std::forward<legacy::body_with_attrs<Fn>>(attrd_body), g, attrd_body.get_attrs());
                }

            private:
                legacy::body_with_attrs<Fn>& _b;

                /// Creates tasks from fn and the attributes and adds it to group.
                /// @param fn Code that the task will execute.
                /// @param g Group the task should be added to.
                /// @param t_attr attributes of the created task.
                /// @return task*  Pointer to the newly created task.
                static task* create_task_internal(Fn&& fn, group* g, legacy::task_attrs t_attr)
                {
                    typedef typename internal::task_type_info<Fn, true> task_type_info;
                    typedef typename task_type_info::user_code          user_code;

#if !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
                    char* task_buffer = task_allocator::allocate(sizeof(cputask<task_type_info>));
                    auto  t           = new (task_buffer) cputask<task_type_info>(g, t_attr, std::forward<user_code>(fn));
#else
                    auto t = new cputask<task_type_info>(g, t_attr, std::forward<user_code>(fn));
#endif // !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)

                    return t;
                }

                /// Creates tasks from fn and adds it to group.
                /// @param fn Code and atrributes for the new task.
                /// @param g Group the task should be added to.
                /// @return task*  Pointer to the newly created task.
                static task* create_task_internal(legacy::body_with_attrs<Fn>&& attrd_body, group* g, legacy::task_attrs t_attr)
                {
                    typedef typename internal::task_type_info<Fn, true> task_type_info;
                    typedef typename task_type_info::user_code user_code;

#if !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
                    char* task_buffer = task_allocator::allocate(sizeof(cputask<task_type_info>));
                    auto  t = new (task_buffer) cputask<task_type_info>(g, t_attr, std::forward<user_code>(attrd_body.get_body()));
#else
                    auto t = new cputask<task_type_info>(g, t_attr, std::forward<user_code>(attrd_body.get_body()));
#endif // !defined(HETCOMPUTE_NO_TASK_ALLOCATOR)
                    return t;
                }
            }; // struct body_wrapper_base<body_with_attrs>

#ifdef HETCOMPUTE_HAVE_GPU
            /// Template specialization for gpu task.
            template <typename Fn, typename Kernel, typename... Args>
            struct body_wrapper_base<legacy::body_with_attrs_gpu<Fn, Kernel, Args...>>
            {
                typedef function_traits<Fn>                                                          traits;
                typedef typename legacy::body_with_attrs_gpu<Fn, Kernel, Args...>::kernel_parameters kparams;
                typedef typename legacy::body_with_attrs_gpu<Fn, Kernel, Args...>::kernel_arguments  kargs;

                /// Returns function, lambda or member function that was wrapped in
                /// legacy::body_with_attrs.
                /// \return
                ///  Fn -- Function, lambda or member function used as body.
                HETCOMPUTE_CONSTEXPR Fn& get_fn() const { return _b.get_body(); }

                /// Returns attributes from legacy::body_with_attrs.
                /// \return
                ///  legacy::task_attrs Attributes from legacy::body_with_attrs.
                HETCOMPUTE_CONSTEXPR legacy::task_attrs get_attrs() const { return _b.get_attrs(); }

                /// Constructor
                /// \param b legacy::body_with_attrs wrapper
                HETCOMPUTE_CONSTEXPR explicit body_wrapper_base(const legacy::body_with_attrs_gpu<Fn, Kernel, Args...>&& b) : _b(b) {}

                /// Creates tasks from fn and adds it to group.
                /// \param fn Code and atrributes for the new task.
                /// \param g Group the task should be added to.
                /// \return
                /// task*  Pointer to the newly created task.
                template <size_t Dims>
                static task* create_task(legacy::device_ptr const&                          device,
                                         const ::hetcompute::range<Dims>&                     r,
                                         const ::hetcompute::range<Dims>&                     l,
                                         legacy::body_with_attrs_gpu<Fn, Kernel, Args...>&& attrd_body)
                {
                    auto attrs = attrd_body.get_attrs();
                    return new gputask<Dims, Fn, kparams, kargs>(device,
                                                                 r,
                                                                 l,
                                                                 attrd_body.get_body(),
                                                                 attrs,
                                                                 attrd_body.get_gpu_kernel(),
                                                                 attrd_body.get_cl_kernel_args());
                }

            private:
                const legacy::body_with_attrs_gpu<Fn, Kernel, Args...>& _b;
            };
#endif // HETCOMPUTE_HAVE_GPU

            /// Wraps functions, lambdas, and member functions used
            /// to create tasks, even when they are also wrapped
            /// within with_attrs.
            template <typename Body>
            struct body_wrapper : public body_wrapper_base<Body>
            {
                typedef typename body_wrapper_base<Body>::traits body_traits;
                typedef typename body_traits::return_type        return_type;

                // VS does not recognize constexpr, so we cannot
                // use func<get_arity(), Fn> to instantiate the template.
                // Example: hetcompute/internal/patterns/adaptivepfor.hh
                static const size_t s_arity = body_traits::arity::value;

                /// Returns number of arguments required by body.
                /// @return size_t body arity.
                HETCOMPUTE_CONSTEXPR static size_t get_arity() { return s_arity; }

                /// Invokes body.
                /// @param args Body arguments.
                /// @return return_type Returns whatever the body returns.
                template <typename... Args>
                HETCOMPUTE_CONSTEXPR return_type operator()(Args... args) const
                {
                    return (body_wrapper_base<Body>::get_fn())(args...);
                }

                /// Constructor.
                /// @param b Body to be wrapped.
                HETCOMPUTE_CONSTEXPR explicit body_wrapper(Body&& b) : body_wrapper_base<Body>(std::forward<Body>(b)) {}
            }; // struct body_wrapper<Body>

            /// \brief Easier way to get a body_wrapper
            /// \return
            /// body_wrapper<B> A body_wrapper object for b
            template<typename B>
            static body_wrapper<B> get_body_wrapper(B&& b)
            {
                return body_wrapper<B>(std::forward<B>(b));
            }

            /// Removes constness and references from type T
            template <typename T>
            struct remove_cv_and_reference
            {
                typedef typename std::remove_cv<typename std::remove_reference<T>::type>::type type;
            };

            /// If T is a task_ptr, this struct provides the member constant value equal true.
            template <typename T>
            struct is_task_ptr
                : std::integral_constant<bool, std::is_same<hetcompute::internal::task_shared_ptr, typename remove_cv_and_reference<T>::type>::value>
            {
            };

            /// If T is a task_ptr, this struct provides the member constant value equal true.
            template <class T>
            struct is_hetcompute_task_ptr : std::integral_constant<bool, is_task_ptr<T>::value>
            {
            };

            /// Internally called when the task was launched using launch(task_ptr)
            template <typename T, typename = typename std::enable_if<is_hetcompute_task_ptr<T>::value>::type>
            inline void launch_dispatch(hetcompute::internal::group_shared_ptr const& a_group, T const& a_task, bool notify = true)
            {
                HETCOMPUTE_UNUSED(notify);
                auto t_ptr = c_ptr(a_task);
                auto g_ptr = c_ptr(a_group);

                HETCOMPUTE_API_ASSERT(t_ptr, "null task was passed.");
                t_ptr->launch(g_ptr, nullptr);
            }

            /// Called when the task was launched using launch(internal::group_shared_ptr, body), note that body is not task.
            /// So, create a task first, then launch.
            template <typename Body, typename = typename std::enable_if<!is_hetcompute_task_ptr<Body>::value>::type>
            inline void launch_dispatch(hetcompute::internal::group_shared_ptr const& a_group, Body&& body, bool notify = true)
            {
                auto g_ptr = c_ptr(a_group);
                HETCOMPUTE_API_ASSERT(g_ptr, "null group was passed.");

                hetcompute::internal::legacy::body_wrapper<Body> wrapper(std::forward<Body>(body));
                auto                                             attrs = wrapper.get_attrs();

                // if the body is not blocking, just create a task and send to runtime, otherwise use task::launch().
                if (!has_attr(attrs, hetcompute::internal::legacy::attr::blocking))
                {
                    attrs   = legacy::add_attr(attrs, hetcompute::internal::attr::anonymous);
                    task* t = wrapper.create_task(g_ptr, attrs);
                    g_ptr->inc_task_counter();
                    hetcompute::internal::send_to_runtime(t, nullptr, notify);
                }
                else
                {
                    task* t = wrapper.create_task(g_ptr, attrs);
                    t->launch(g_ptr, nullptr);
                }
            }

            /// Called when the task was launched using launch(task_ptr)
            template <typename T, typename = typename std::enable_if<is_hetcompute_task_ptr<T>::value>::type>
            inline void launch_dispatch(T const& task)
            {
                static hetcompute::internal::group_shared_ptr null_group_ptr;
                hetcompute::internal::legacy::launch_dispatch(null_group_ptr, task);
            }

        }; // namespace legacy

    }; // namespace internal

}; // namespace hetcompute
