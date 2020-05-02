#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <tuple>
#include <type_traits>

#include <hetcompute/internal/buffer/buffer-internal.hh>
#include <hetcompute/internal/buffer/bufferstate.hh>
#include <hetcompute/internal/compat/compat.h>
#include <hetcompute/internal/compat/compiler_compat.h>
#include <hetcompute/internal/task/task.hh>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/strprintf.hh>
#include <hetcompute/internal/util/templatemagic.hh>

namespace hetcompute
{
    namespace internal
    {
        // Forward declaration
        class arena;

        void create_dynamic_dependence_from_timed_task(task* requestor_task);

        /// Derived policy class of bufferpolicy will:
        /// - Have knowledge of what arena-types are available on system
        /// - Construct arena objects and add to a buffer's bufferstate
        /// - Service action-requests from the scheduler about a given buffer
        ///   by invoking the buffer's bufferstate APIs
        class bufferpolicy
        {
        public:
            // Action-types that can be requested by the scheduler from the policy
            enum action_t
            {
                acquire_r,
                acquire_w,
                acquire_rw,
                release
            };

            /// A special requestor ID value for the host. No actual pointer will have this value.
            /// A buffer acquired using this requestor value indicates that the buffer has been acquired by the host.
            static constexpr size_t s_host_requestor_id = 0xee;

            virtual ~bufferpolicy()
            {
            }

            /// Returns the unique type of arena that a task executing on ed
            /// must use to access a buffer's data.
            virtual arena_t get_arena_type_accessed_by_device(executor_device ed) = 0;

            /// Gets or creates an arena suitable for access by the CPU.
            /// When make_data_valid == true, also ensures that arena has valid data.
            ///
            /// Does not lock/unlock bufstate. Calling context must ensure either:
            /// i)  non-concurrent invocation with any method on the same bufstate object, or
            /// ii) lock bufstate.
            ///
            /// Pre-condition when make_data_valid == true:
            ///   Calling context must ensure that the call will succeed:
            ///    - there will be no copy-conflicts if a data copy to mainmem-arena is required.
            ///   A sufficient condition is for the caller to check that the requestor-set of this
            ///  buffer is empty when this call is made.
            virtual arena* unsafe_get_or_create_cpu_arena(bufferstate* bufstate, size_t size_in_bytes, bool make_data_valid) = 0;

            /// Creates an arena suitable for access by the CPU using initial storage and data from user.
            /// Error if a CPU-accessible arena already exists.
            /// Error if the buffer already contains valid data.
            ///
            /// Does not lock/unlock bufstate. Calling context must ensure either:
            /// i)  non-concurrent invocation with any method on the same bufstate object, or
            /// ii) lock bufstate.
            virtual arena* unsafe_create_arena_with_cpu_initial_data(bufferstate* bufstate, void* p, size_t size_in_bytes) = 0;

            /// Creates an arena of a suitable type depending on the internal type of the provided memregion.
            /// Error if an arena for the given memregion type already exists.
            /// Error if the buffer already contains valid data.
            ///
            /// Does not lock/unlock bufstate. Calling context must ensure either:
            /// i)  non-concurrent invocation with any method on the same bufstate object, or
            /// ii) lock bufstate.
            virtual arena*
            unsafe_create_arena_with_memregion_initial_data(bufferstate* bufstate, internal_memregion* int_mr, size_t size_in_bytes) = 0;

            /// Defines scope of buffer acquisition in request_acquire_action().
            /// Summary:
            ///   Acquisition of a buffer prior to use by compute-code is either done in
            ///    - one step with acquire_scope::full,
            ///    - or, two steps with acquire_scope::tentative + acquire_scope::confirm.
            /// Details below.
            enum class acquire_scope
            {
                /// Tentatively reserve the buffer for a requestor, don't attempt to acquire an arena yet.
                /// Preferred scope when it is not yet guaranteed that the requestor will imminently make
                /// use of the buffer, and may relinquish the buffer without use. Requestor must
                /// confirm the reservation to acquire an arena.
                tentative,

                /// Verifies that a buffer has previously been tentatively reserved for a requestor,
                /// then marks the reservation as confirmed and acquires a suitable arena.
                confirm,

                /// Reserves the buffer for a requestor and also acquires a suitable arena.
                full
            };

            /// Return value type for request_acquire_action().
            ///
            /// Semantics:
            /// _no_conflict_found == true ==>
            ///    _conflicting_requestor == nullptr
            ///    _acquire_multiplicity gives acquire multiplicity of requestor in request_acquire_action()
            ///    _acquired_arena_per_device holds the successfully acquired arenas for the
            ///          corresponding requested executor devices if arena acquire was in the scope
            ///          for request_acquire_action() (i.e., with acquire_scope::confirm or acquire_scope::full),
            ///          otherwise, all arena entries are  == nullptr (i.e., with acquire_scope::tentative).
            ///
            /// _no_conflict_found == false ==>
            ///    _conflicting_requestor identifies the conflicting requestor that is
            ///          the *confirmed* user of the buffer, otherwise, == nullptr indicates
            ///          that the conflict is with a tentative user of the buffer.
            ///    _acquire_multiplicity gives acquire multiplicity of _conflicting_requestor if
            ///          _conflicting_requestor != nullptr, o.w., =0
            ///    _acquired_arena_per_device has nullptr entries for all arenas.
            struct conflict_info
            {
                bool                   _no_conflict_found;
                void const*            _conflicting_requestor;
                size_t                 _acquire_multiplicity;
                per_device_arena_array _acquired_arena_per_device;

                conflict_info() : _no_conflict_found(true), _conflicting_requestor(nullptr), _acquire_multiplicity(0), _acquired_arena_per_device()
                {
                }

                conflict_info(bool no_conflict_found, void const* conflicting_requestor, size_t acquire_multiplicity)
                    : _no_conflict_found(no_conflict_found), _conflicting_requestor(conflicting_requestor), _acquire_multiplicity(acquire_multiplicity), _acquired_arena_per_device()
                {
                }

                std::string to_string() const
                {
                    std::string arenas_string = std::string("[");
                    for (auto i = static_cast<size_t>(executor_device::first); i <= static_cast<size_t>(executor_device::last); i++)
                    {
                        arenas_string.append(hetcompute::internal::strprintf("%p", _acquired_arena_per_device[i]));
                        if (i < static_cast<size_t>(executor_device::last))
                        {
                            arenas_string.append(", ");
                        }
                    }
                    arenas_string.append("]");
                    return hetcompute::internal::strprintf("{%s, arenas=%s, conflicting_requestor=%p}",
                                                         (_no_conflict_found ? "no_conflict" : "conflict"),
                                                         arenas_string.c_str(),
                                                         _conflicting_requestor);
                }
            };

            /// Reserves a buffer for a requestor if no conflicts found.
            /// Additionally, if acquire_scope != tentative, acquires suitable arenas for the requested executor devices,
            /// including synchronizing the buffer data and setting up the internal arena
            /// state to make the acauired arenas accessible to the requestor on the requested executor devices.
            ///
            /// Return semantics: see conflict_info
            ///
            /// Locks and unlocks bufstate, if lock_bufstate == true.
            /// Use lock_bufstate == false only when it can be guaranteed
            /// that there is no concurrent access to bufstate
            /// (e.g., for a locally created buffer that will be used with only one task).
            virtual conflict_info request_acquire_action(bufferstate*                  bufstate,
                                                         void const*                   requestor,
                                                         executor_device_bitset const& edb,
                                                         action_t                      ac,
                                                         acquire_scope                 as,
                                                         buffer_as_texture_info        tex_info,
                                                         bool                          lock_bufstate = true) = 0;

            /// A previous acquire request for the given requestor and device is
            /// 1. looked up in bufstate,
            /// 2. its multiplicity downgraded (if a confirmed acquirer), and
            /// 3. and released if requestor was a tentative acquirer or multiplicity becomes == 0
            ///
            /// Error if no such acquire request exists.
            ///
            /// Returns remaining multiplicity for requestor in bufstate.
            ///  - Releases bufstate for requestor iff returns 0.
            ///
            /// Locks and unlocks bufstate, if lock_bufstate == true.
            /// Use lock_bufstate == false only when it can be guaranteed
            /// that there is no concurrent access to bufstate
            /// (e.g., for a locally created buffer that will be used with only one task).
            virtual size_t release_action(bufferstate* bufstate, void const* requestor, bool lock_bufstate = true) = 0;

            /// Removes the arena that the policy would preferably pick for the given executor_device
            /// (if such an arena is currently allocated)
            ///
            /// Locks and unlocks bufstate, if lock_bufstate == true.
            /// Use lock_bufstate == false only when it can be guaranteed
            /// that there is no concurrent access to bufstate
            /// (e.g., for a locally created buffer that will be used with only one task).
            virtual void remove_matching_arena(bufferstate* bufstate, executor_device ed, bool lock_bufstate = true) = 0;

        }; // class bufferpolicy

        inline std::string to_string(bufferpolicy::action_t ac)
        {
            switch (ac)
            {
            case bufferpolicy::acquire_r:
                return "acqR";
            case bufferpolicy::acquire_w:
                return "acqW";
            case bufferpolicy::acquire_rw:
                return "acqRW";
            case bufferpolicy::release:
                return "rel";
            default:
                return "invalid";
            }
        }

        bufferpolicy* get_current_bufferpolicy();

        template <typename T>
        struct is_api20_buffer_ptr_helper;

        template <typename T>
        struct is_api20_buffer_ptr_helper : public std::false_type
        {
        };

        template <typename T>
        struct is_api20_buffer_ptr_helper<::hetcompute::buffer_ptr<T>> : public std::true_type
        {
        };

        template <typename T>
        struct is_api20_buffer_ptr_helper<::hetcompute::in<::hetcompute::buffer_ptr<T>>> : public std::true_type
        {
        };

        template <typename T>
        struct is_api20_buffer_ptr_helper<::hetcompute::out<::hetcompute::buffer_ptr<T>>> : public std::true_type
        {
        };

        template <typename T>
        struct is_api20_buffer_ptr_helper<::hetcompute::inout<::hetcompute::buffer_ptr<T>>> : public std::true_type
        {
        };

        template <typename T>
        struct is_api20_buffer_ptr : public is_api20_buffer_ptr_helper<typename strip_ref_cv<T>::type>
        {
        };

        template <typename BufPtr>
        struct is_const_buffer_ptr_helper : public std::false_type
        {
        };

        template <typename T>
        struct is_const_buffer_ptr_helper<::hetcompute::buffer_ptr<const T>> : public std::true_type
        {
        };

        template <typename T>
        struct is_const_buffer_ptr : public is_const_buffer_ptr_helper<typename strip_ref_cv<T>::type>
        {
        };

        template <typename Tuple, size_t index>
        struct num_buffer_ptrs_in_tuple_helper
        {
            using elem = typename std::tuple_element<index - 1, Tuple>::type;
            enum
            {
                value = (is_api20_buffer_ptr<elem>::value ? 1 : 0) + num_buffer_ptrs_in_tuple_helper<Tuple, index - 1>::value
            };
        };

        template <typename Tuple>
        struct num_buffer_ptrs_in_tuple_helper<Tuple, 0>
        {
            enum
            {
                value = 0
            };
        };

        template <typename Tuple>
        struct num_buffer_ptrs_in_tuple
        {
            enum
            {
                value = num_buffer_ptrs_in_tuple_helper<Tuple, std::tuple_size<Tuple>::value>::value
            };
        };

        template <typename... Args>
        struct num_buffer_ptrs_in_args;

        template <typename Arg, typename... Args>
        struct num_buffer_ptrs_in_args<Arg, Args...>
        {
            enum
            {
                value = (is_api20_buffer_ptr<Arg>::value ? 1 : 0) + num_buffer_ptrs_in_args<Args...>::value
            };
        };

        template <>
        struct num_buffer_ptrs_in_args<>
        {
            enum
            {
                value = 0
            };
        };

        /// Adds a new buffer entry to buffer_acquire_set::_arr_buffers, checking or extending depending on type of _arr_buffers.
        /// Implementations specialized for the std::vector and std::array cases.
        template <typename BuffersArrayType>
        struct checked_addition_of_buffer_entry;

        template <typename BufferInfo>
        struct checked_addition_of_buffer_entry<std::vector<BufferInfo>>
        {
            static void add(std::vector<BufferInfo>& arr_buffers, size_t& num_buffers_added, BufferInfo const& bi)
            {
                HETCOMPUTE_INTERNAL_ASSERT(arr_buffers.size() == num_buffers_added, "new entry expected to be added only at end");
                arr_buffers.push_back(bi);
                num_buffers_added = arr_buffers.size();
            }
        };

        template <typename BufferInfo, size_t MaxNumBuffers>
        struct checked_addition_of_buffer_entry<std::array<BufferInfo, MaxNumBuffers>>
        {
            static void add(std::array<BufferInfo, MaxNumBuffers>& arr_buffers, size_t& num_buffers_added, BufferInfo const& bi)
            {
                HETCOMPUTE_INTERNAL_ASSERT(num_buffers_added < MaxNumBuffers,
                                         "buffer_acquire_set: adding beyond expected max size: _num_buffers_added=%zu must be < "
                                         "MaxNumBuffers=%zu",
                                         num_buffers_added,
                                         MaxNumBuffers);

                arr_buffers[num_buffers_added++] = bi;
            }
        };

        /// Resizes buffer_acquire_set::_acquired_arenas when statically sized.
        template <typename ArenasArrayType>
        struct resize_arenas_array
        {
            static void resize(size_t arr_buffers_size, ArenasArrayType& acquired_arenas)
            {
                HETCOMPUTE_INTERNAL_ASSERT(arr_buffers_size == acquired_arenas.size(),
                                         "Fixed-size acquired_arenas (size=%zu) must have same size as final arr_buffers.size()=%zu",
                                         acquired_arenas.size(),
                                         arr_buffers_size);
                for (auto& ma : acquired_arenas)
                {
                    for (auto& a : ma)
                    {
                        a = nullptr;
                    }
                }
            }
        };

        /// Resizes buffer_acquire_set::_acquired_arenas when dynamically sized.
        template <size_t MultiDeviceCount>
        struct resize_arenas_array<std::vector<std::array<arena*, MultiDeviceCount>>>
        {
            static void resize(size_t arr_buffers_size, std::vector<std::array<arena*, MultiDeviceCount>>& acquired_arenas)
            {
                acquired_arenas.resize(arr_buffers_size);
                for (auto& ma : acquired_arenas)
                {
                    for (auto& a : ma)
                    {
                        a = nullptr;
                    }
                }
            }
        };

        /**
         *  Gives the search result of a lookup of a bufferstate in a buffer_entity_container.
         *
         *  Entity is an arbitrary value type that can be saved in buffer_entity_container<Entity>
         *  and can be searched for in the container, with the search result provided by
         *  buffer_entity_finder<Entity>.
        */
        template<typename Entity>
        class buffer_entity_finder
        {
        private:
            // whether a desired entity was found
            bool _found;

            // value of found entity, if found
            // defined iff _found == true
            Entity _entity;

            // index into container inside buffer_entity_container giving location of entity, if found
            // defined iff _found == true
            size_t _index;

        public:
            /**
             *  Search result when entity was not found
             */
            buffer_entity_finder() : _found(false), _entity(), _index(0)
            {
            }

            /**
             *  Search result when entity was found
             */
            buffer_entity_finder(Entity found_entity, size_t found_index) : _found(true), _entity(found_entity), _index(found_index)
            {
            }

            bool found() const
            {
                return _found;
            }

            Entity entity() const
            {
                HETCOMPUTE_INTERNAL_ASSERT(_found, "No entity has been found.");
                return _entity;
            }

            size_t index() const
            {
                HETCOMPUTE_INTERNAL_ASSERT(_found, "No entity has been found.");
                return _index;
            }

            // copies and moves allowed
            HETCOMPUTE_DEFAULT_METHOD(buffer_entity_finder(buffer_entity_finder const&));
            HETCOMPUTE_DEFAULT_METHOD(buffer_entity_finder& operator=(buffer_entity_finder const&));
            HETCOMPUTE_DEFAULT_METHOD(buffer_entity_finder(buffer_entity_finder&&));
            HETCOMPUTE_DEFAULT_METHOD(buffer_entity_finder& operator=(buffer_entity_finder&&));
        };  // class buffer_entity_finder

        /*
         *  Container to hold an ordered collection of entities associated with bufferstates.
         *  Multiple entities can be added for the same bufferstate. It is the responsibility
         *  of the client code to enforce correctness properties, e.g., ensuring only a single
         *  entity is added for a bufferstate, if so required for a particular Entity type.
         *
         *  @tparam Entity         An arbitrary value type stored by buffer_entity_container<Entity>.
         *
         *  @tparam FixedSize      Allows specializations of the container to be either
         *                         statically or dynamically sized.
         *
         *  @tparam MaxNumEntities When FixedSize == true, provides maximum static size.
         *                         Ignored if FixedSize == false.
        */
        template <typename Entity, bool FixedSize, size_t MaxNumEntities = 0>
        class buffer_entity_container;

        /**
         *  Statically sized entity container
        */
        template <typename Entity, size_t MaxNumEntities>
        class buffer_entity_container<Entity, true, MaxNumEntities>
        {
        private:
            using container = std::array<std::pair<bufferstate*, Entity>, MaxNumEntities>;

            // Helps avoid allocating potentially a large storage until it is clear that at least
            // one entity needs to be saved. Optimized for the common case of no entities stored.
            // Penalty is a single dynamic allocation when there is an entity to store.
            std::unique_ptr<container> _container;

        public:
            buffer_entity_container() : _container(nullptr)
            {
            }

            /**
             *  @return whether entity could be saved
             */
            bool add_buffer_entity(bufferstate* bufstate, Entity& entity)
            {
                HETCOMPUTE_INTERNAL_ASSERT(bufstate != nullptr, "null buffer");
                if (_container == nullptr)
                {
                    _container.reset(new container);
                    for (auto& p : *_container)
                    {
                        p.first = nullptr;
                    }
                }

                bool saved = false;
                for (auto& p : *_container)
                {
                    if (p.first == nullptr)
                    {
                        p     = std::make_pair(bufstate, entity);
                        saved = true;
                        break;
                    }
                }
                return saved;
            }

            buffer_entity_finder<Entity> find_buffer_entity(bufferstate* bufstate, size_t start_index = 0) const
            {
                HETCOMPUTE_INTERNAL_ASSERT(bufstate != nullptr, "null buffer");
                if (_container == nullptr)
                {
                    return buffer_entity_finder<Entity>(); // not found
                }

                for (size_t i = start_index; i < (*_container).size(); i++)
                {
                    auto const& p = (*_container)[i];
                    if (p.first == bufstate)
                    {
                        return buffer_entity_finder<Entity>(p.second, i); // found at i
                    }

                    if (p.first == nullptr)
                    {
                        return buffer_entity_finder<Entity>(); // not found
                    }
                }
                return buffer_entity_finder<Entity>(); // not found
            }

            bool has_any() const
            {
                return (_container != nullptr);
            }

            // forbid copies
            HETCOMPUTE_DELETE_METHOD(buffer_entity_container(buffer_entity_container const&));
            HETCOMPUTE_DELETE_METHOD(buffer_entity_container& operator=(buffer_entity_container const&));

            // moves allowed
            HETCOMPUTE_DEFAULT_METHOD(buffer_entity_container(buffer_entity_container&&));
            HETCOMPUTE_DEFAULT_METHOD(buffer_entity_container& operator=(buffer_entity_container&&));
        };

        /**
         *  Dynamically sized entity container
         */
        template <typename Entity, size_t MaxNumEntities>
        class buffer_entity_container<Entity, false, MaxNumEntities>
        {
            using container = std::vector<std::pair<bufferstate*, Entity>>;
            container _container;

        public:
            buffer_entity_container() : _container()
            {
            }

            /**
             *  @return whether entity could be saved
             */
            bool add_buffer_entity(bufferstate* bufstate, Entity& entity)
            {
                HETCOMPUTE_INTERNAL_ASSERT(bufstate != nullptr, "null buffer");
                _container.push_back(std::make_pair(bufstate, entity));
                return true;
            }

            buffer_entity_finder<Entity> find_buffer_entity(bufferstate* bufstate, size_t start_index = 0) const
            {
                HETCOMPUTE_INTERNAL_ASSERT(bufstate != nullptr, "null buffer");
                for (size_t i = start_index; i < _container.size(); i++)
                {
                    auto const& p = _container[i];
                    if (p.first == bufstate)
                    {
                        return buffer_entity_finder<Entity>(p.second, i); // found at i
                    }

                    if (p.first == nullptr)
                    {
                        return buffer_entity_finder<Entity>(); // not found
                    }
                }
                return buffer_entity_finder<Entity>(); // not found
            }

            bool has_any() const
            {
                return (!_container.empty());
            }

            // forbid copies
            HETCOMPUTE_DELETE_METHOD(buffer_entity_container(buffer_entity_container const&));
            HETCOMPUTE_DELETE_METHOD(buffer_entity_container& operator=(buffer_entity_container const&));

            // moves allowed
            HETCOMPUTE_DEFAULT_METHOD(buffer_entity_container(buffer_entity_container&&));
            HETCOMPUTE_DEFAULT_METHOD(buffer_entity_container& operator=(buffer_entity_container&&));
        };

        /**
         * Search result for arenas inside an instance of preacquired_arenas
         */
        using preacquired_arena_finder = buffer_entity_finder<arena*>;

        /**
         *  Virtual base class to provide a uniform lookup interface for
         *  the various instances of preacquired_arenas.
         *
         *  Helps decouple the various templated variations of preacquired_arenas
         *  from the various templated variations of buffer_acquire_set,
         *  so the two variations can be mixed freely as needed by the use-cases.
         *
         *  @sa preacquired_arenas
         */
        class preacquired_arenas_base
        {
        public:
            /**
             *  A given bufstate may be saved multiple times to allow multiple
             *  preacquired arenas for different executor devices to be provided
             *  for a single buffer.
             */
            virtual void register_preacquired_arena(bufferstate* bufstate, arena* preacquired_arena) = 0;

            /**
             *  Use the start_index parameter to search progressively for additional
             *  occurrences of the same bufstate. The prior preacquired_arena_finder
             *  value 'paf' has index() method to provide the index of the prior occurence
             *  in the container ==> use start_index = paf.index()+1 for progressive search.
             */
            virtual preacquired_arena_finder find_preacquired_arena(bufferstate* bufstate, size_t start_index = 0) const = 0;

            virtual bool has_any() const = 0;

            virtual ~preacquired_arenas_base()
            {
            }
        };

        /**
         *  Allows preacquired arenas to be specified for buffer arguments to a task.
         *  @sa task::unsafe_register_preacquired_arena()
        */
        template <bool FixedSize, size_t MaxNumArenas = 0>
        class preacquired_arenas : public preacquired_arenas_base
        {
        private:
            buffer_entity_container<arena*, FixedSize, MaxNumArenas> _bec;

        public:
            preacquired_arenas() : _bec()
            {
            }

            void register_preacquired_arena(bufferstate* bufstate, arena* preacquired_arena)
            {
                HETCOMPUTE_INTERNAL_ASSERT(preacquired_arena != nullptr, "null arena");

                bool saved = _bec.add_buffer_entity(bufstate, preacquired_arena);
                HETCOMPUTE_API_ASSERT(saved, "unable to save preacquired_arena=%p for bufstate=%p", preacquired_arena, bufstate);
            }

            preacquired_arena_finder find_preacquired_arena(bufferstate* bufstate, size_t start_index = 0) const
            {
                return _bec.find_buffer_entity(bufstate, start_index);
            }

            bool has_any() const
            {
                return _bec.has_any();
            }

            // forbid copies
            HETCOMPUTE_DELETE_METHOD(preacquired_arenas(preacquired_arenas const&));
            HETCOMPUTE_DELETE_METHOD(preacquired_arenas& operator=(preacquired_arenas const&));

            // moves allowed
            HETCOMPUTE_DEFAULT_METHOD(preacquired_arenas(preacquired_arenas&&));
            HETCOMPUTE_DEFAULT_METHOD(preacquired_arenas& operator=(preacquired_arenas&&));
        };

        /**
         * Search result for override devices inside an instance of override_device_sets
         */
        using override_device_set_finder = buffer_entity_finder<executor_device_bitset>;

        /**
         *  Virtual base class to provide a uniform lookup interface for
         *  the various instances of override_device_sets.
         *
         *  Helps decouple the various templated variations of override_device_sets
         *  from the various templated variations of buffer_acquire_set,
         *  so the two variations can be mixed freely as needed by the use-cases.
         *
         *  @sa override_device_sets
        */
        class override_device_sets_base
        {
        public:
            /**
             *  A given bufstate may be saved only once.
             */
            virtual void register_override_device_set(bufferstate* bufstate, executor_device_bitset edb) = 0;

            /**
             *  At most a single occurence of bufstate will be found.
             */
            virtual override_device_set_finder find_override_device_set(bufferstate* bufstate) const = 0;

            virtual ~override_device_sets_base()
            {
            }
        };

        /**
         *  Allows override_device_sets to be specified for buffer arguments to a task.
         */
        template <bool FixedSize, size_t MaxNumBuffers = 0>
        class override_device_sets : public override_device_sets_base
        {
        private:
            buffer_entity_container<executor_device_bitset, FixedSize, MaxNumBuffers> _bec;

        public:
            override_device_sets() : _bec()
            {
            }

            void register_override_device_set(bufferstate* bufstate, executor_device_bitset override_edb)
            {
                HETCOMPUTE_INTERNAL_ASSERT(!override_edb.has(executor_device::unspecified) && override_edb.count() > 0,
                                         "Need at least one valid device in override_edb=%s",
                                         internal::to_string(override_edb).c_str());

                // bufstate should not already be present
                HETCOMPUTE_INTERNAL_ASSERT(_bec.find_buffer_entity(bufstate).found() == false,
                                         "Only one override executor_device_bitset may be specified for a buffer: bufstate=%p already "
                                         "contains one",
                                         bufstate);

                bool saved = _bec.add_buffer_entity(bufstate, override_edb);
                HETCOMPUTE_API_ASSERT(saved,
                                    "unable to save override edb=%s for bufstate=%p",
                                    internal::to_string(override_edb).c_str(),
                                    bufstate);
            }

            override_device_set_finder find_override_device_set(bufferstate* bufstate) const
            {
                auto odf = _bec.find_buffer_entity(bufstate, 0);

                // a given bufstate may occur at most once in the container
                HETCOMPUTE_INTERNAL_ASSERT(!odf.found() || _bec.find_buffer_entity(bufstate, odf.index() + 1).found() == false,
                                         "Only one override executor_device_bitset may be specified for a buffer: found multiple for "
                                         "bufstate=%p",
                                         bufstate);
                return odf;
            }

            bool has_any() const
            {
                return _bec.has_any();
            }

            HETCOMPUTE_DELETE_METHOD(override_device_sets(override_device_sets const&));
            HETCOMPUTE_DELETE_METHOD(override_device_sets& operator=(override_device_sets const&));
        };

        /**
         *  Printer.
         *  @sa buffer_acquire_set::_multi_ed
        */
        template <size_t MultiDeviceCount>
        std::string to_string(std::array<hetcompute::internal::executor_device, MultiDeviceCount> const& multi_ed)
        {
            std::string s = std::string("[");
            for (auto ed : multi_ed)
            {
                s.append(internal::to_string(ed) + " ");
            }
            s.append("]");
            return s;
        }

        /**
         *  acquire status for buffer_acquire_set.
         *
         *  Reflects the status of acquiring all buffers within a buffer_acquire_set.
        */
        enum class acquire_status
        {
            idle,                 // no acquire request underway or completed
            tentatively_acquired, // all buffers tentatively acquired for a requestor
            fully_acquired        // all buffers have been acquired for a requestor to use
        };

        /// buffer_acquire_set purpose:
        /// 1. Allows the entire collection of buffer arguments for a task to be
        ///    acquired all at once or not at all (releases the partial set on failure).
        /// 2. Acquires the buffers in a sorted order guaranteed to avoid deadlock with
        ///    buffer-acquisitions by a concurrent task.
        ///    In general, we need to take care to avoid deadlocks in the following situations:
        ///      task A: acquire buf1, then acquire buf2
        ///      task B: acquire buf2, then acquire buf1
        ///
        ///  FixedSize == true  ==> rely on upfront knowledge of MaxNumBuffers with std::array
        ///  FixedSize == false ==> use dynamically extendable std::vector (MaxNumBuffers ignored)
        ///
        ///  MultiDeviceCount is an upper bound on the number of executor devices a requestor may use.
        template <size_t MaxNumBuffers, bool FixedSize = true, size_t MultiDeviceCount = 1>
        class buffer_acquire_set
        {
        private:
            using action_t = hetcompute::internal::bufferpolicy::action_t;

            // Used to flag that a buffer is tentatively reserved, doesn't have an acquired arena yet.
            // See _acquired_arenas.
            static constexpr size_t s_flag_tentative_acquire = 0x1;

            // Used to fill an entry in _acquired_arenas[i] when _arr_buffers[i] has been
            // acquired for fewer devices than the buffer acquire set as a whole.
            // Specific buffers have exceptions specified for their executor_device_bitset
            // via the p_override_device_sets parameter to acquire_buffers()
            static constexpr size_t s_flag_fake_arena = 0x2;

            struct buffer_info
            {
                bufferstate*           _bufstate_raw_ptr;
                action_t               _acquire_action;
                buffer_as_texture_info _tex_info;
                bool                   _uses_preacquired_arena;

                buffer_info()
                    : _bufstate_raw_ptr(nullptr),
                      _acquire_action(action_t::acquire_r),
                      _tex_info(),
                      _uses_preacquired_arena(false)
                {
                }

                buffer_info(bufferstate*           bufstate_raw_ptr,
                            action_t               acquire_action,
                            buffer_as_texture_info tex_info = buffer_as_texture_info())
                    : _bufstate_raw_ptr(bufstate_raw_ptr),
                      _acquire_action(acquire_action),
                      _tex_info(tex_info),
                      _uses_preacquired_arena(false)
                {
                }

                std::string to_string() const
                {
                    return hetcompute::internal::strprintf("(bs=%p %s)",
                                                         _bufstate_raw_ptr,
                                                         internal::to_string(_acquire_action).c_str());
                }
            }; // struct buffer_info

            using buffers_array =
                typename std::conditional<FixedSize, std::array<buffer_info, MaxNumBuffers>, std::vector<buffer_info>>::type;

            using multidevice_arena = std::array<arena*, MultiDeviceCount>;

            using arenas_array =
                typename std::conditional<FixedSize, std::array<multidevice_arena, MaxNumBuffers>, std::vector<multidevice_arena>>::type;

            // Retains the multiple executor devices from the last acquire_buffers() call, upto MultiDeviceCount devices.
            // Elements at indices idev = 0, 1, ... hold valid devices until first element with executor_device::unspecified.
            std::array<hetcompute::internal::executor_device, MultiDeviceCount> _multi_ed;

            // Array of buffers to be acquired for requestor.
            buffers_array _arr_buffers;

            // Arenas acquired for corresponding entries of _arr_buffers.
            // _acquired_arenas[i] is an array of arenas acquired for the buffer _arr_buffers[i],
            // where the arenas correspond to the one or more valid devices in _multi_ed.
            //
            // If _arr_buffers (once sorted), has multiple occurrences of the same bufferstate,
            // then only the last occurrence of the bufferstate will get non-null arenas in the corresponding
            // _acquired_arenas entry.
            //
            // A non-null entry is interpreted as follows:
            //  == s_flag_tentative_acquire for _multi_ed[0] ==> the corresponding buffer has been tentatively reserved,
            //    no arena acquired yet.
            //  == s_flag_fake_arena for _multi_ed[idev] when the corresponding buffer has been acquired for fewer devices
            //    than the buffer acquire set as a whole (via the p_override_device_sets parameter to acquire_buffers()),
            //    with _multi_ed[idev] being a device skipped for this buffer.
            //  Otherwise, gives the acquired arenas for each valid device in _multi_ed.
            arenas_array _acquired_arenas;

            // Number of valid entries in _arr_buffers
            size_t _num_buffers_added;

            // see enum class acquire_status
            acquire_status _acquire_status;

            // =true  => lock/unlock buffers during acquire/release steps.
            // =false => do not lock/unlock.
            // Use _lock_buffers == false only when it can be guaranteed
            // that there is no concurrent access to the buffers in _arr_buffers
            // (e.g., for locally created buffers that will be accessed by only one task at a time).
            bool _lock_buffers;

            /// Refer to enum class acquire_status
            inline acquire_status get_acquire_status() const
            {
                return _acquire_status;
            }

            /// Helper function for acquire_buffers() :
            ///  - used for initialization and setup
            void acquire_precheck_and_setup(hetcompute::internal::executor_device_bitset edb)
            {
                HETCOMPUTE_INTERNAL_ASSERT(get_acquire_status() == acquire_status::idle,
                                         "acquire_buffers(): buffers are already acquired or a prior acquisition is underway");
                HETCOMPUTE_INTERNAL_ASSERT(edb.count() > 0, "No executor device provided");
                HETCOMPUTE_INTERNAL_ASSERT(edb.count() <= MultiDeviceCount,
                                         "buffer_acquire_set can hold only %zu executor devices, request with %zu devices, edb=%s",
                                         MultiDeviceCount,
                                         edb.count(),
                                         internal::to_string(edb).c_str());
                HETCOMPUTE_INTERNAL_ASSERT(!edb.has(executor_device::unspecified),
                                         "unspecified device type cannot be used: edb=%s",
                                         internal::to_string(edb).c_str());

                // Set up _multi_ed from edb
                size_t ed_count = 0;
                edb.for_each([&](executor_device ed) {
                    HETCOMPUTE_INTERNAL_ASSERT(ed == executor_device::cpu || ed == executor_device::gpucl || ed == executor_device::gpugl ||
                                                 ed == executor_device::dsp,
                                             "ed = %zu is not an executor device type for a task.",
                                             static_cast<size_t>(ed));
                    _multi_ed[ed_count++] = ed;
                });

                for (; ed_count < MultiDeviceCount; ed_count++)
                {
                    _multi_ed[ed_count] = executor_device::unspecified;
                }

                // Sort ascending by bufferstate*. Will also group multiple occurrences of the same buffer together.
                std::sort(_arr_buffers.begin(), _arr_buffers.begin() + _num_buffers_added, [](buffer_info const& v1, buffer_info const& v2) {
                    return v1._bufstate_raw_ptr < v2._bufstate_raw_ptr;
                });

                resize_arenas_array<arenas_array>::resize(_arr_buffers.size(), _acquired_arenas);
            }

            /// Helper function for acquire_buffers() :
            ///  - used when a task will access preacquired arenas for a given buffer
            void assign_preacquired_arenas_to_devices_for_single_buffer(bufferpolicy*                                     bp,
                                                                        hetcompute::internal::executor_device_bitset const& edb_to_use,
                                                                        bufferstate*                                      bs,
                                                                        preacquired_arenas_base const* p_preacquired_arenas,
                                                                        bufferpolicy::conflict_info&   conflict)
            {
#ifdef HETCOMPUTE_CHECK_INTERNAL
                // First, assert that no arenas have been assigned to devices yet for this buffer.
                for (size_t idev = 0; idev < _multi_ed.size(); idev++)
                {
                    auto ed  = _multi_ed[idev];
                    auto ied = static_cast<size_t>(ed);
                    if (ed == executor_device::unspecified)
                    {
                        break; // no more valid devices in _multi_ed
                    }

                    HETCOMPUTE_INTERNAL_ASSERT(conflict._acquired_arena_per_device[ied] == nullptr,
                                             "Expected no arenas to have been assigned per device as yet. bufstate=%p ed=%zu",
                                             bs,
                                             static_cast<size_t>(ed));
                }
#endif // HETCOMPUTE_CHECK_INTERNAL

                // Attempt to give a suitable preacquired arena to every executor device in _multi_ed.
                preacquired_arena_finder paf = p_preacquired_arenas->find_preacquired_arena(bs);
                while (paf.found())
                {
                    auto preacquired_arena = paf.entity();
                    // brute force search into _multi_ed, but usually _multi_ed.size() == 1, and upto 3.
                    for (size_t idev = 0; idev < _multi_ed.size(); idev++)
                    {
                        auto ed = _multi_ed[idev];
                        HETCOMPUTE_INTERNAL_ASSERT(preacquired_arena != nullptr, "preacquired_arena cannot be nullptr.");
                        if (ed == executor_device::unspecified)
                        {
                            break; // no more valid devices in _multi_ed
                        }

                        if (bp->get_arena_type_accessed_by_device(ed) == arena_state_manip::get_type(preacquired_arena))
                        {
                            auto ied = static_cast<size_t>(ed);
                            // edb_to_use may be a strict subset of _multi_ed. If so, skip the preacquired_arena for extraneous devices
                            if (edb_to_use.has(ed))
                            {
                                conflict._acquired_arena_per_device[ied] = preacquired_arena;
                            }
                        }
                    } // for-loop over _multi_ed
                    // attempt to find additional arenas
                    paf = p_preacquired_arenas->find_preacquired_arena(bs, paf.index() + 1);
                } // while-loop over preacquired arenas for bs

#ifdef HETCOMPUTE_CHECK_INTERNAL
                // Lastly, verify that every valid device in edb_to_use has a preacquired arena
                edb_to_use.for_each([&](executor_device ed) {
                    auto ied = static_cast<size_t>(ed);
                    HETCOMPUTE_INTERNAL_ASSERT(conflict._acquired_arena_per_device[ied] != nullptr &&
                                                 conflict._acquired_arena_per_device[ied] !=
                                                     reinterpret_cast<arena*>(s_flag_tentative_acquire) &&
                                                 conflict._acquired_arena_per_device[ied] != reinterpret_cast<arena*>(s_flag_fake_arena),
                                             "Expected a preacquired arena for every device in edb_to_use=%s for bufstate=%p",
                                             internal::to_string(edb_to_use).c_str(),
                                             bs);
                });
#endif // HETCOMPUTE_CHECK_INTERNAL
            }

            /// Helper function for acquire_buffers() :
            ///  - used when a task must acquire/synchronize arenas from inside a buffer to access the buffer data.
            void acquire_single_buffer_or_find_conflict(bufferpolicy*                                     bp,
                                                        void const*                                       requestor,
                                                        hetcompute::internal::executor_device_bitset const& specialized_edb,
                                                        bufferstate*                                      bs,
                                                        buffer_as_texture_info&                           tex_info,
                                                        action_t const                                    ac,
                                                        size_t const                                      pass,
                                                        bool const                                        setup_task_deps_on_conflict,
                                                        bufferpolicy::conflict_info&                      conflict)
            {
                bool retry_buffer_acquire = false;
                do
                {
                    conflict = bp->request_acquire_action(bs,
                                                          requestor,
                                                          specialized_edb,
                                                          ac,
                                                          (pass == 1 ? bufferpolicy::acquire_scope::tentative :
                                                                       bufferpolicy::acquire_scope::confirm),
                                                          tex_info,
                                                          _lock_buffers);
                    if (conflict._no_conflict_found == false)
                    {
                        // encountered acquire conflict, all previously acquired buffers must be released at toplevel
                        HETCOMPUTE_INTERNAL_ASSERT(pass == 1, "buffer conflicts are expected to be found only in pass 1");

                        if (!setup_task_deps_on_conflict)
                        {
                            HETCOMPUTE_INTERNAL_ASSERT(conflict._no_conflict_found == false, "Violated invariant");
                            return; // conflict found
                        }

                        // spin until a confirmed conflicting requestor is identified for the buffer
                        while (conflict._no_conflict_found == false && conflict._conflicting_requestor == nullptr)
                        {
                            conflict =
                                bp->request_acquire_action(bs, requestor, specialized_edb, ac, bufferpolicy::acquire_scope::tentative, tex_info);
                        }

                        // Returns if conflict found on buffer (possible only during pass 1 -- tentative acquires).
                        // May also spin-retry and find that conflict is no longer present ==> okay to proceed in that case.
                        if (conflict._no_conflict_found == false)
                        {
                            // Conflict persists and the conflicting requestor is now confirmed.
                            HETCOMPUTE_INTERNAL_ASSERT(conflict._conflicting_requestor != nullptr, "Should have remained in while loop!");

                            // Note, _conflicting_requestor may have already released buffer by now.
                            // But it doesn't matter, we will setup task control dependence anyways.
                            // (no harm in pretending to still have a conflict)

                            auto conflicting_task = static_cast<task*>(const_cast<void*>(conflict._conflicting_requestor));
                            HETCOMPUTE_INTERNAL_ASSERT(conflicting_task != nullptr,
                                                     "Conflict was confirmed, but conflicting requestor was not identified");
                            auto current_task = static_cast<task*>(const_cast<void*>(requestor));
                            HETCOMPUTE_INTERNAL_ASSERT(current_task != nullptr, "Requestor task not specified");
                            HETCOMPUTE_INTERNAL_ASSERT(conflicting_task != current_task, "Buffer already held by the same task");

                            if (conflicting_task != reinterpret_cast<void*>(bufferpolicy::s_host_requestor_id) &&
                                current_task != reinterpret_cast<void*>(bufferpolicy::s_host_requestor_id))
                            { // Verified: both conflicting_task and current_task are actually tasks ==> dependence can be set up

                                // The control dependence addition has to be *guaranteed*,
                                // i.e., either the predecessor has not completed execution and is
                                // guaranteed to trigger the successor, or the predecessor was found
                                // already completed and the return status will indicate that.
                                if (conflicting_task->add_dynamic_control_dependency(current_task))
                                {
                                    // Dynamic dep was added. Release any tentatively acquired
                                    // buffers at the toplevel.
                                    HETCOMPUTE_INTERNAL_ASSERT(conflict._no_conflict_found == false, "Violated invariant");
                                    return; // conflict found
                                }
                                else
                                {
                                    // Conflicting task has already finished, and released buffer
                                    // (since buffers are released before task transitions to
                                    // finished). Retry buffer acquisition.
                                    retry_buffer_acquire = true;
                                }
                            }
                            else if (current_task != reinterpret_cast<void*>(bufferpolicy::s_host_requestor_id))
                            { // Verified: current_task is an actual task, but conflicting_task is not
                                create_dynamic_dependence_from_timed_task(current_task);
                                return; // conflict found
                            }
                            else
                            {
                                HETCOMPUTE_FATAL("acquire_buffers(): dynamic dependences cannot be used for non-task requestor");
                            }
                        }
                        else
                        {
                            // tentative conflict has disappeared, reset retry buffer acquire flag.
                            retry_buffer_acquire = false;
                        }
                    }
                    else
                    {
                        // No conflict, reset retry buffer acquire flag.
                        retry_buffer_acquire = false;
                    }
                } while (retry_buffer_acquire);

                HETCOMPUTE_INTERNAL_ASSERT(conflict._no_conflict_found == true, "Violated invariant");
                return; // no conflict found
            }

            // Make two passes over _arr_buffers.
            // Pass 1:
            //   - tentatively acquire all the buffers, stop and undo at first conflict.
            //   - if setup_task_deps_on_conflict == true, spin on buffer acquisition until
            //     the conflicting requestor is confirmed (i.e., becomes != nullptr), and
            //     then set up a task control dependence from the conflicting requestor.
            //   - return false if a conflict found, true otherwise.
            // Pass 2:
            //   - executed only if no conflicts found in Pass 1.
            //   - confirm acquisition of buffers, acquire arenas and save the acquired arenas.
            //   - Pass 2 will always return true, as no conflicts can be found.
            bool attempt_acquire_pass(size_t const                                      pass,
                                      bufferpolicy*                                     bp,
                                      void const*                                       requestor,
                                      hetcompute::internal::executor_device_bitset const& edb,
                                      bool const                                        setup_task_deps_on_conflict,
                                      preacquired_arenas_base const*                    p_preacquired_arenas,
                                      override_device_sets_base const*                  p_override_device_sets)
            {
                size_t index = 0;
                while (index < _num_buffers_added)
                {
                    auto bs       = _arr_buffers[index]._bufstate_raw_ptr;
                    auto ac       = _arr_buffers[index]._acquire_action;
                    auto tex_info = _arr_buffers[index]._tex_info;

                    auto edb_to_use = edb;
                    if (p_override_device_sets != nullptr)
                    {
                        override_device_set_finder odf = p_override_device_sets->find_override_device_set(bs);
                        if (odf.found())
                        {
                            edb_to_use = odf.entity();

#ifdef HETCOMPUTE_CHECK_INTERNAL
                            // Ensure edb_to_use is a subset of edb:
                            // - We actually want to ensure that edb_to_use is a subset of _multi_ed[], and
                            // - we assume that _multi_ed and edb have the same devices.
                            edb_to_use.for_each([&](executor_device ed) {
                                HETCOMPUTE_INTERNAL_ASSERT(edb.has(ed),
                                                         "override edb=%s must be a subset of general edb=%s, bs=%p",
                                                         internal::to_string(edb_to_use).c_str(),
                                                         internal::to_string(edb).c_str(),
                                                         bs);
                            });
#endif // HETCOMPUTE_CHECK_INTERNAL
                        }
                    }

                    // Consume all occurrences of the same bufferstate
                    while (index + 1 < _num_buffers_added && bs == _arr_buffers[index + 1]._bufstate_raw_ptr)
                    {
                        // only the last index for the same bufferstate will hold the acquired arena
                        for (auto& a : _acquired_arenas[index])
                        {
                            a = nullptr;
                        }

                        auto next_ac   = _arr_buffers[index + 1]._acquire_action;
                        if (ac != next_ac)
                        {
                            // pick superset acquire action
                            ac = action_t::acquire_rw;

                            // Explanation:
                            // ac == acquire_r ==> next_ac = acquire_w or acquire_rw ==> superset = acquire_rw
                            // ac == acquire_w ==> next_ac = acquire_r or acquire_rw ==> superset = acquire_rw
                            // ac == acquire_rw ==> superset = acquire_rw
                        }
                        index++;
                    }

                    // setup conflict data structure
                    //  - captures arenas assigned to particular devices for the buffer bs.
                    //  - captures cause of conflict otherwise.
                    //  - class buffer_acquire_set ignores multiplicity field of conflict_info, put it as 0
                    bufferpolicy::conflict_info conflict{ true, nullptr, 0 };

                    // Has a preacquired arena been provided for this buffer?
                    bool any_preacquired_arena_for_buffer =
                        (p_preacquired_arenas != nullptr && p_preacquired_arenas->find_preacquired_arena(bs).found());
                    if (any_preacquired_arena_for_buffer)
                    {
                        // Use only preacquired arenas from p_preacquired_arenas for this buffer:
                        //   - completely skip expensive buffer acquisition (involving lock/unlock) for this buffer.
                        //   - every valid device in _multi_ed must have a preacquired_arena specified for this buffer.
                        //   - note that the buffer_acquire_set skips arena ref() / unref() for preacquired arenas.
                        //      Those must be handled appropriately outside the buffer_acquire_set functionality.
                        if (pass == 2)
                        {
                            assign_preacquired_arenas_to_devices_for_single_buffer(bp, edb_to_use, bs, p_preacquired_arenas, conflict);
                            HETCOMPUTE_INTERNAL_ASSERT(conflict._no_conflict_found == true,
                                                     "No conflicts expected for a buffer with preacquired arenas. bufstate=%p",
                                                     bs);
                        }
                    }
                    else
                    {
                        // convert gpucl to more specialized gputexture when the buffer is to be used as OpenCL texture
                        executor_device_bitset specialized_edb;
                        edb_to_use.for_each([&](executor_device ed) {
                            specialized_edb.add(
                                (ed == executor_device::gpucl && tex_info.get_used_as_texture()) ? executor_device::gputexture : ed);
                        });

                        // Acquire arenas for devices from inside the bufferstate (requires lock/unlock of bufferstate).
                        // None of the devices in _multi_ed get a preacquired_arena for this buffer.
                        acquire_single_buffer_or_find_conflict(bp,
                                                               requestor,
                                                               specialized_edb,
                                                               bs,
                                                               tex_info,
                                                               ac,
                                                               pass,
                                                               setup_task_deps_on_conflict,
                                                               conflict);
                    }
                    _arr_buffers[index]._uses_preacquired_arena = any_preacquired_arena_for_buffer;

                    if (conflict._no_conflict_found == false)
                    {
                        return false; // conflict found
                    }

                    // Now, no conflict found on buffer

                    if (pass == 2)
                    {
                        // fill with fake arenas for devices in _multi_ed skipped by edb_to_use
                        for (size_t idev = 0; idev < _multi_ed.size(); idev++)
                        {
                            auto ed = _multi_ed[idev];
                            if (ed == executor_device::unspecified)
                            {
                                break; // no more valid devices in _multi_ed
                            }

                            auto ied = static_cast<size_t>(ed);
                            if (!edb_to_use.has(ed) &&
                                (conflict._acquired_arena_per_device[ied] == nullptr ||
                                 conflict._acquired_arena_per_device[ied] == reinterpret_cast<arena*>(s_flag_tentative_acquire)))
                            {
                                conflict._acquired_arena_per_device[ied] = reinterpret_cast<arena*>(s_flag_fake_arena);
                            }
                        }
                    }

                    for (size_t idev = 0; idev < MultiDeviceCount; idev++)
                    {
                        auto ed = _multi_ed[idev];
                        HETCOMPUTE_INTERNAL_ASSERT(idev > 0 || ed != executor_device::unspecified,
                                                 "Expected at least one valid requested executor device");
                        if (ed == executor_device::unspecified)
                        {
                            break;
                        }

                        auto specialized_ed =
                            (ed == executor_device::gpucl && tex_info.get_used_as_texture()) ? executor_device::gputexture : ed;
                        if (pass == 1)
                        {
                            HETCOMPUTE_INTERNAL_ASSERT(conflict._acquired_arena_per_device[static_cast<size_t>(specialized_ed)] == nullptr,
                                                     "Pass 1 should not have acquired_arena (for ed=%zu) on tentative acquire of "
                                                     "bufstate=%p",
                                                     static_cast<size_t>(specialized_ed),
                                                     bs);
                            _acquired_arenas[index][idev] = reinterpret_cast<arena*>(s_flag_tentative_acquire);
                        }
                        else if (pass == 2)
                        {
                            // last occurrence of same bufferstate holds arena acquired from that bufferstate (for each requested executor
                            // device)
                            HETCOMPUTE_INTERNAL_ASSERT(conflict._acquired_arena_per_device[static_cast<size_t>(specialized_ed)] != nullptr,
                                                     "Pass 2 should have found valid acquired_arena (for ed=%zu) on confirmed acquire of "
                                                     "bufstate=%p",
                                                     static_cast<size_t>(specialized_ed),
                                                     bs);
                            _acquired_arenas[index][idev] = conflict._acquired_arena_per_device[static_cast<size_t>(specialized_ed)];
                        }
                        else
                        {
                            HETCOMPUTE_UNREACHABLE("There should only be Pass 1 and Pass 2. Found pass=%zu", pass);
                        }
                    }

                    index++; // process next buffer
                }            // while loop over _arr_buffers

                if (pass == 1)
                {
                    // no conflicts found
                    _acquire_status = acquire_status::tentatively_acquired;
                    ;
                }
                else if (pass == 2)
                {
                    // all the arenas were acquired for all the buffers without conflict
                    _acquire_status = acquire_status::fully_acquired;
                }
                else
                {
                    HETCOMPUTE_UNREACHABLE("Invalid pass = %zu", pass);
                }

                return true; // no conflict found
            }

        public:
            buffer_acquire_set()
                : _multi_ed(),
                  _arr_buffers(),
                  _acquired_arenas(),
                  _num_buffers_added(0),
                  _acquire_status(acquire_status::idle),
                  _lock_buffers(true)
            {
            }

            /** Instructs the buffer acquire set to skip lock/unlock on the
             *  contained buffers during acquire/release operations.
             *  (default: acquire/release of buffers will do locks/unlocks).
            */
            void enable_non_locking_buffer_acquire()
            {
                _lock_buffers = false;
            }

            /**
             * Add the individual buffers
            */
            template <typename BufferPtr>
            void add(BufferPtr& b, action_t acquire_action, buffer_as_texture_info tex_info = buffer_as_texture_info())
            {
                static_assert(is_api20_buffer_ptr<BufferPtr>::value, "must be a hetcompute::buffer_ptr");
                if (b == nullptr)
                {
                    return;
                }

                auto bufstate         = buffer_accessor::get_bufstate(reinterpret_cast<buffer_ptr_base&>(b));
                auto bufstate_raw_ptr = ::hetcompute::internal::c_ptr(bufstate);
                HETCOMPUTE_INTERNAL_ASSERT(bufstate_raw_ptr != nullptr, "Non-null buffer_ptr contains a null bufferstate");

                checked_addition_of_buffer_entry<buffers_array>::add(_arr_buffers,
                                                                     _num_buffers_added,
                                                                     buffer_info{ bufstate_raw_ptr,
                                                                                  acquire_action,
                                                                                  tex_info });
            }

            /**
             *  The number of (non unique) buffers added to this buffer acquire set.
            */
            size_t get_num_buffers_added() const
            {
                return _num_buffers_added;
            }

            /**
             *  =true  => all the buffers have been acquired for a requestor (via acquire_buffers())
             *  =false => none of the buffers have been acquired for a requestor:
             *    - Either acquire_buffers() has not yet been called on this buffer_acquire_set,
             *    - or, only a tentative acquire has been requested so far,
             *    - or, a conflict was found.
            */
            inline bool acquired() const
            {
                return (get_acquire_status() == acquire_status::fully_acquired);
            }

            /**
             *  Acquires all the buffers or none at all. Non-blocking.
             *  Sets _acquire_status = true if all acquired, o.w. false.
             *  @param requestor The unique task* of the task wanting to acquire its buffer arguments,
             *                   which are listed in _arr_buffers.
             *  @param edb       The set of one or more devices where the task will execute.
             *  @param setup_task_deps_on_conflict
             *                   =false ==> On finding a buffer conflict, existing buffers are released.
             *                   =true  ==> Additionally, a task control dependence is set up from whichever
             *                              task has currently acquired the conflicting buffer.
             *
             *  @param p_preacquired_arenas
             *                   Pre-acquired arenas for some subset of buffers in this buffer_acquire_set.
             *                   Buffer acquire/release and acquisition of arenas is skipped when a
             *                   pre-acquired arena is provided for a buffer.
             *                   == nullptr ==> no pre-acquired arenas provided.
             *
             *  @param p_override_device_sets
             *                   Overrides specific buffers to be acquired for different devices
             *                   than those in edb. However, the override devices must be a subset of edb.
            */
            void acquire_buffers(void const*                                requestor,
                                 hetcompute::internal::executor_device_bitset edb,
                                 const bool                                 setup_task_deps_on_conflict = false,
                                 preacquired_arenas_base const*             p_preacquired_arenas        = nullptr,
                                 override_device_sets_base const*           p_override_device_sets      = nullptr)
            {
                acquire_precheck_and_setup(edb);

                auto bp = get_current_bufferpolicy();

                // Pass 1: attempt to tentatively acquire all buffers for requestor, identify conflicts
                // Pass 2: confirm buffer acquires if Pass 1 succeeded.
                for (size_t pass = 1; pass <= 2; pass++)
                {
                    bool no_conflict =
                        attempt_acquire_pass(pass, bp, requestor, edb, setup_task_deps_on_conflict, p_preacquired_arenas, p_override_device_sets);
                    HETCOMPUTE_INTERNAL_ASSERT(pass == 1 || no_conflict, "Pass 2 is not expected to encounter conflicts.");
                    if (!no_conflict)
                    { // abort on conflict
                        // release any tentatively acquired buffers
                        release_buffers(requestor);
                        return;
                    }
                }

                log::fire_event<log::events::buffer_set_acquired>();
            }

            /**
             *  Similar to acquire_buffers(), except will retry until buffers acquired.
            */
            void blocking_acquire_buffers(void const*                                requestor,
                                          hetcompute::internal::executor_device_bitset edb,
                                          preacquired_arenas_base const*             p_preacquired_arenas   = nullptr,
                                          override_device_sets_base const*           p_override_device_sets = nullptr)
            {
                size_t spin_count = 10;
                while (!acquired())
                {
                    HETCOMPUTE_INTERNAL_ASSERT(get_acquire_status() != acquire_status::tentatively_acquired,
                                             "blocking_acquire_buffers(): at this granularity there should be no tentative acquire. bas=%p",
                                             this);
                    if (spin_count > 0)
                    {
                        spin_count--;
                    }
                    else
                    {
                        usleep(1);
                    }

                    acquire_buffers(requestor, edb, false, p_preacquired_arenas, p_override_device_sets);
                }
            }

            /**
             * Releases any buffers acquired so far. Buffers may be only tentatively acquired, fully
             * acquired or not acquired at all.
            */
            void release_buffers(void const* requestor)
            {
                HETCOMPUTE_INTERNAL_ASSERT(_multi_ed[0] != executor_device::unspecified,
                                         "There must be at least one valid executor device to acquire buffers");

                auto bp = get_current_bufferpolicy();

                for (size_t i = 0; i < _num_buffers_added; i++)
                {
                    // sufficient to check for device index 0 since there is at least one valid executor device _multi_ed[0]
                    if (_acquired_arenas[i][0] == nullptr)
                    {
                        continue;
                    }

                    auto bufstate_raw_ptr       = _arr_buffers[i]._bufstate_raw_ptr;
                    bool uses_preacquired_arena = _arr_buffers[i]._uses_preacquired_arena;

                    if (!uses_preacquired_arena)
                    {
                        auto acquire_multiplicity = bp->release_action(bufstate_raw_ptr, requestor, _lock_buffers);
                        HETCOMPUTE_API_ASSERT(acquire_multiplicity == 0,
                                              "HetCompute currently does not support non-task entities (tasks, patterns) to "
                                              "acquire buffers with multiplicity");
                    }

                    for (auto& a : _acquired_arenas[i])
                    {
                        a = nullptr;
                    }
                }

                _acquire_status = acquire_status::idle;

                // Invariant: now all entries _acquired_arenas[i][j] == nullptr
            }

            // when MultiDeviceCount == 1, using ed = unspecified will return the one arena acquired for the buffer.
            // when MultiDeviceCount > 1, ed must be a valid device in _multi_ed.
            template <typename BufferPtr>
            arena* find_acquired_arena(BufferPtr& b, hetcompute::internal::executor_device ed = hetcompute::internal::executor_device::unspecified)
            {
                HETCOMPUTE_INTERNAL_ASSERT(get_acquire_status() == acquire_status::fully_acquired,
                                         "buffer_acquire_set::find_acquired_arena(): Attempting to find"
                                         "arena when buffers not acquired");

                static_assert(is_api20_buffer_ptr<BufferPtr>::value, "must be a hetcompute::buffer_ptr");
                if (b == nullptr)
                {
                    return nullptr;
                }

                auto bufstate         = buffer_accessor::get_bufstate(reinterpret_cast<hetcompute::internal::buffer_ptr_base&>(b));
                auto bufstate_raw_ptr = ::hetcompute::internal::c_ptr(bufstate);

                if (MultiDeviceCount == 1 && ed == hetcompute::internal::executor_device::unspecified)
                {
                    ed = _multi_ed[0]; // the only device used
                }

                HETCOMPUTE_INTERNAL_ASSERT(ed != hetcompute::internal::executor_device::unspecified, "Invalid executor device");

                size_t idev = 0;
                for (; idev < MultiDeviceCount; idev++)
                {
                    if (_multi_ed[idev] == ed)
                    {
                        break;
                    }
                }
                HETCOMPUTE_API_ASSERT(idev < MultiDeviceCount,
                                    "executor device ed=%zu was not used to acquire buffers: failure on bufstate=%p, multi_ed=%s",
                                    static_cast<size_t>(ed),
                                    bufstate_raw_ptr,
                                    internal::to_string(_multi_ed).c_str());
                HETCOMPUTE_INTERNAL_ASSERT(_multi_ed[idev] == ed, "Impossible! Linear search is kaput.");

                for (size_t i = 0; i < _num_buffers_added; i++)
                {
                    if (_arr_buffers[i]._bufstate_raw_ptr == bufstate_raw_ptr && _acquired_arenas[i][idev] != nullptr)
                    {
                        HETCOMPUTE_INTERNAL_ASSERT(_acquired_arenas[i][idev] != reinterpret_cast<arena*>(s_flag_tentative_acquire),
                                                 "Only tentative arena found: bs=%p ed=%s",
                                                 _arr_buffers[i]._bufstate_raw_ptr,
                                                 internal::to_string(ed).c_str());

                        // buffer acquire may have been skipped for some devices ==> no arenas
                        if (_acquired_arenas[i][idev] == reinterpret_cast<arena*>(s_flag_fake_arena))
                        {
                            return nullptr;
                        }

                        return _acquired_arenas[i][idev];
                    }
                }
                return nullptr;
            }

            std::string to_string() const
            {
                std::string s = hetcompute::internal::strprintf("bas %p %s num_buffers_added=%zu acquired=%c locking=%c\n",
                                                              this,
                                                              (FixedSize ? "FixedSize" : "DynSize"),
                                                              _num_buffers_added,
                                                              (acquired() ? 'Y' : 'N'),
                                                              (_lock_buffers ? 'Y' : 'N'));
                s.append("  buffers = [");
                for (auto const& buf_info : _arr_buffers)
                {
                    s.append(buf_info.to_string() + " ");
                }
                s.append("]\n");

                if (acquired())
                {
                    s.append("  acquired arenas :\n");
                    for (size_t idev = 0; idev < _multi_ed.size(); idev++)
                    {
                        auto ed = _multi_ed[idev];
                        if (ed == executor_device::unspecified)
                        {
                            break; // no more valid devices in _multi_ed
                        }

                        s.append("    " + internal::to_string(ed) + " : ");
                        for (size_t i = 0; i < _acquired_arenas.size(); i++)
                        {
                            HETCOMPUTE_INTERNAL_ASSERT(idev < _acquired_arenas[i].size(),
                                                     "idev=%zu must be < _acquired_arenas[%zu].size()=%zu",
                                                     idev,
                                                     i,
                                                     _acquired_arenas[i].size());
                            s.append(hetcompute::internal::strprintf("%p ", _acquired_arenas[i][idev]));
                        }
                        s.append("\n");
                    }
                }

                return s;
            }
        }; // class buffer_acquire_set

        /**
         * template helpers for stripping the hetcompute::buffer direction
         * general case
        */
        template <typename BufferWithDir>
        struct strip_buffer_dir
        {
            using type = BufferWithDir;
        };

        /**
         * template helpers for stripping the hetcompute::buffer direction
         * case for hetcompute::buffer with direction specified
         */
        template <typename T>
        struct strip_buffer_dir<hetcompute::out<hetcompute::buffer_ptr<T>>>
        {
            using type = hetcompute::buffer_ptr<T>;
        };

        /**
         * template helpers for stripping the hetcompute::buffer direction
         * case for hetcompute::buffer with direction specified
         */
        template <typename T>
        struct strip_buffer_dir<hetcompute::in<hetcompute::buffer_ptr<T>>>
        {
            using type = hetcompute::buffer_ptr<T>;
        };

        /**
         * template helpers for stripping the hetcompute::buffer direction
         * case for hetcompute::buffer with direction specified
         */
        template <typename T>
        struct strip_buffer_dir<hetcompute::inout<hetcompute::buffer_ptr<T>>>
        {
            using type = hetcompute::buffer_ptr<T>;
        };

    }; // namespace internal
};     // namespace hetcompute
