#pragma once

#include <array>
#include <condition_variable>
#include <cmath>
#include <mutex>
#include <set>
#include <string>

#include <hetcompute/devicetypes.hh>

#include <hetcompute/internal/buffer/arenaaccess.hh>
#include <hetcompute/internal/buffer/executordevice.hh>
#include <hetcompute/internal/buffer/memregion-internal.hh>
#include <hetcompute/internal/compat/compat.h>
#include <hetcompute/internal/compat/compiler_compat.h>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/macros.hh>
#include <hetcompute/internal/util/hetcomputeptrs.hh>
#include <hetcompute/internal/util/strprintf.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace testing
        {
            class BufferStateTest;
        };
    };
};

namespace hetcompute
{
    namespace internal
    {
        // captures arena corresponding to each executor device type.
        // indexed by executor_device enum.
        using per_device_arena_array = std::array<arena*, static_cast<size_t>(executor_device::last) + 1>;

        inline std::string to_string(per_device_arena_array const& pdaa)
        {
            std::string s = std::string("[");
            for (auto a : pdaa)
            {
                s.append(::hetcompute::internal::strprintf("%p ", a));
            }
            s.append("]");
            return s;
        }

        struct buffer_acquire_info
        {
        public:
            enum access_t
            {
                unspecified,
                read,
                write,
                readwrite
            };

            static inline std::string to_string(access_t access)
            {
                return std::string(access == unspecified ? "U" : (access == read ? "R" : (access == write ? "W" : "RW")));
            }

            void* _acquired_by;

            /// defined iff _acquired_by != nullptr
            executor_device_bitset _edb;

            /// defined iff _acquired_by != nullptr
            access_t _access;

            /// defined iff _acquired_by != nullptr
            bool _tentative_acquire;

            /// defined iff _acquired_by != nullptr
            per_device_arena_array _acquired_arena_per_device;

            /// defined iff _acquired_by != nullptr
            size_t _acquire_multiplicity;

            buffer_acquire_info()
                : _acquired_by(nullptr), _edb(), _access(unspecified), _tentative_acquire(false), _acquired_arena_per_device(), _acquire_multiplicity(0)
            {
            }

            explicit buffer_acquire_info(void const* acquired_by)
                : _acquired_by(const_cast<void*>(acquired_by)),
                  _edb(),
                  _access(unspecified),
                  _tentative_acquire(false),
                  _acquired_arena_per_device(),
                  _acquire_multiplicity(0)
            {
            }

            explicit buffer_acquire_info(void const* acquired_by, executor_device_bitset const& edb, size_t acquire_multiplicity)
                : _acquired_by(const_cast<void*>(acquired_by)),
                  _edb(edb),
                  _access(unspecified),
                  _tentative_acquire(false),
                  _acquired_arena_per_device(),
                  _acquire_multiplicity(acquire_multiplicity)
            {
            }

            buffer_acquire_info(void const* acquired_by, executor_device_bitset edb, access_t access, bool tentative_acquire, size_t acquire_multiplicity)
                : _acquired_by(const_cast<void*>(acquired_by)),
                  _edb(edb),
                  _access(access),
                  _tentative_acquire(tentative_acquire),
                  _acquired_arena_per_device(),
                  _acquire_multiplicity(acquire_multiplicity)
            {
            }

            std::string to_string() const
            {
                return ::hetcompute::internal::strprintf("{%p, %s, %s, %s, %s, #%zu}",
                                                       _acquired_by,
                                                       ::hetcompute::internal::to_string(_edb).c_str(),
                                                       this->to_string(_access).c_str(),
                                                       (_tentative_acquire ? "T" : "F"),
                                                       ::hetcompute::internal::to_string(_acquired_arena_per_device).c_str(),
                                                       _acquire_multiplicity);
            }
        };  // struct buffer_acquire_info

        bool operator<(buffer_acquire_info const& bai1, buffer_acquire_info const& bai2);

        /// Used to search for an buffer_acquire_info entry for a given requestor
        inline bool operator<(buffer_acquire_info const& bai1, buffer_acquire_info const& bai2)
        {
            return bai1._acquired_by < bai2._acquired_by;
        }

        /// Statistics: updatable mean and variance as samples are added
        class sample_mean_var
        {
        private:
            size_t _count;
            double _mean;
            double _var;

        public:
            sample_mean_var() : _count(0), _mean(0.0), _var(0.0)
            {
            }

            void add_sample(double val)
            {
                double new_mean = (_mean * _count + val) / (_count + 1);

                // Reference: "Computational formula for variance"
                double new_var = ((_var + _mean * _mean) * _count + val * val) / (_count + 1) - new_mean * new_mean;

                _mean = new_mean;
                _var  = new_var;
                _count++;
            }

            size_t get_count() const
            {
                return _count;
            }

            double get_mean() const
            {
                return _mean;
            }

            double get_var() const
            {
                return _var;
            }
        };  // class sample_mean_var

        /// Collects performance statistics about a buffer, if requested.
        struct buffer_statistics
        {
            using arena_pair_copy_stats = sample_mean_var;
            using dst_column_t          = std::array<arena_pair_copy_stats, NUM_ARENA_TYPES>;
            using table_t               = std::array<dst_column_t, NUM_ARENA_TYPES>;

            /// _table[i][j] captures statistics for arena-copies from arena-type i to arena-type j
            table_t _table;

            buffer_statistics() : _table()
            {
            }
        };  // struct buffer_statistics

        /// Captures information about the source arena to be used for copying data between arenas in a bufferstate.
        /// Three cases are possible:
        ///  - Source arena has been identified:
        ///     _has_copy_conflict = false
        ///     _src_arena = the source arena
        //
        ///  - No arena with valid data exists:
        ///     _has_copy_conflict = false
        ///     _src_arena = nullptr
        ///
        ///  - Arenas with valid data exist, but their current state precludes a copy
        ///     _has_copy_conflict = true
        ///     _src_arena = nullptr
        struct copy_from_arena_info
        {
            bool   _has_copy_conflict;
            arena* _src_arena;

            bool has_copy_conflict() const
            {
                HETCOMPUTE_INTERNAL_ASSERT(!_has_copy_conflict || _src_arena == nullptr,
                                         "Cannot have a source arena when a copy conflict has been detected");
                return _has_copy_conflict;
            }

            arena* src_arena() const
            {
                HETCOMPUTE_INTERNAL_ASSERT(!_has_copy_conflict || _src_arena == nullptr,
                                         "Cannot have a source arena when a copy conflict has been detected");
                return _src_arena;
            }
        };  // struct copy_from_arena_info

        /// bufferstate purpose:
        ///  1. Ref-counted representative for the concept "buffer".
        ///  2. Provides abstraction over device-specific arenas for copying/syncing of data.
        ///     In particular, no device-specific or arena-type-specific knowledge is used.
        ///  3. Tracks arenas with valid data and requests data copies between arenas
        ///     to transfer validity.
        ///  4. Services requests from bufferpolicy while ensuring correct use of arenas.
        ///
        /// Usage notes:
        ///  - bufferstate neither creates arena objects nor allocate their storage,
        ///    the bufferpolicy must do that.
        ///  - However, destruction of bufferstate will delete all allocated arenas.
        ///    Removal of an arena from bufferstate will also deallocate that arena object.
        ///  - Supports multiple reader tasks for buffer, but currently a writer has to be
        ///    exclusive (reflected in the add_acquire_requestor() logic).
        class bufferstate : public ::hetcompute::internal::ref_counted_object<bufferstate>
        {
        private:
            using existing_arenas_t = std::array<arena*, NUM_ARENA_TYPES>;
            using arena_set_t       = std::array<bool, NUM_ARENA_TYPES>;

            size_t const _size_in_bytes;

            /// mutex: for thread-safe access to the buffer state by the policy
            std::mutex _mutex;
            device_set _device_hints;

            /// arenas that exist, but don't necessarily have valid data.
            /// indexed by arena_t.
            existing_arenas_t _existing_arenas;

            /// which of the existing arenas have valid data
            /// Invariant: subset of _existing_arenas
            /// (i.e., _existing_arenas[i] == nullptr => _valid_data_arenas[i] == false)
            /// indexed by arena_t.
            arena_set_t _valid_data_arenas;
            std::set<buffer_acquire_info> _acquire_set;

            /// Condition variable used to signal pending host acquires when they should
            /// retry acquire of this bufstate.
            std::condition_variable _cv;

            /// flags if there are host acquires waiting to be signalled to retry their buffer acquire
            /// Note: the flag is cleared each time the pending acquirers are signalled. A pending host
            /// acquire must set the flag each time it retries and fails to acquire.
            bool _pending_host_acquires;

            /// name to track buffer during debuging
            std::string _name;

            /// holds buffer statistics if requested
            std::unique_ptr<buffer_statistics> _p_stats;

            /// When true ensures _p_stats is allocated and gets updated
            bool _enable_buffer_statistics;

            /// When true prints the buffer statistics in the bufferstate destructor
            bool _print_statistics_at_dealloc;

            friend class buffer_ptr_base;

            // Constructor
            bufferstate(size_t size_in_bytes, device_set const& device_hints)
                : _size_in_bytes(size_in_bytes),
                  _mutex(),
                  _device_hints(device_hints),
                  _existing_arenas(),
                  _valid_data_arenas(),
                  _acquire_set(),
                  _cv(),
                  _pending_host_acquires(false),
                  _name(),
                  _p_stats(nullptr),
                  _enable_buffer_statistics(false),
                  _print_statistics_at_dealloc(false)
            {
                for (auto& a : _existing_arenas)
                {
                    a = nullptr;
                }

                for (auto& va : _valid_data_arenas)
                {
                    va = false;
                }
            }

            // Cannot default construct
            HETCOMPUTE_DELETE_METHOD(bufferstate());

            // Cannot copy bufferstate
            HETCOMPUTE_DELETE_METHOD(bufferstate(bufferstate const&));
            HETCOMPUTE_DELETE_METHOD(bufferstate& operator=(bufferstate const&));

            // Cannot move bufferstate
            HETCOMPUTE_DELETE_METHOD(bufferstate(bufferstate&&));
            HETCOMPUTE_DELETE_METHOD(bufferstate& operator=(bufferstate&&));

        public:
            friend class hetcompute::internal::testing::BufferStateTest;

            /// deletes all arena objects
            ~bufferstate()
            {
                // the buffer_ptr issuing a host acquire must necessarily hold a smart pointer ref to this bufstate,
                // so we should never encounter the situation of bufstate deletion while there are pending host acquires.
                HETCOMPUTE_INTERNAL_ASSERT(!_pending_host_acquires, "bufstate=%p deleted while there are pending host acquires", this);

                if (_print_statistics_at_dealloc)
                {
                    HETCOMPUTE_ILOG("stats: bufstate=%p %s", this, statistics_to_string().c_str());
                }

                for (auto& a : _existing_arenas)
                {
                    if (a != nullptr)
                    {
                        arena_state_manip::delete_arena(a);
                        a = nullptr;
                    }
                }

                for (auto& va : _valid_data_arenas)
                {
                    va = false;
                }
            }

            size_t get_size_in_bytes() const
            {
                return _size_in_bytes;
            }

            /// A policy can lock a buffer's mutex to get exclusive access to the
            /// buffer state and arenas.
            std::mutex& access_mutex()
            {
                return _mutex;
            }

            device_set const& get_device_hints() const
            {
                return _device_hints;
            }

            /// Gets pointer an arena for arena_type if it exists in this buffer.
            /// Returns nullptr if no such arena present.
            inline arena* get(arena_t arena_type) const
            {
                HETCOMPUTE_INTERNAL_ASSERT(arena_type >= 0 && arena_type < NUM_ARENA_TYPES, "Invalid arena_type %zu", arena_type);
                return _existing_arenas[arena_type];
            }

            /// Checks if the buffer contains an arena of the given arena_type.
            inline bool has(arena_t arena_type) const
            {
                return get(arena_type) != nullptr;
            }

            void add_device_hint(device_type d)
            {
                _device_hints.add(d);
            }

            void remove_device_hint(device_type d)
            {
                _device_hints.remove(d);
            }

            /// indicates if buffer contains valid data in one or more of the existing arenas
            inline bool buffer_holds_valid_data() const
            {
                for (auto va : _valid_data_arenas)
                {
                    if (va == true)
                    {
                        return true;
                    }
                }
                return false;
            }

            size_t num_valid_data_arenas() const
            {
                size_t count = 0;
                for (auto va : _valid_data_arenas)
                {
                    if (va == true)
                    {
                        count++;
                    }
                }
                return count;
            }

            /// arena must already have been created and storage-allocated in this buffer.
            bool is_valid_data_arena(arena* a) const
            {
                HETCOMPUTE_INTERNAL_ASSERT(a != nullptr, "arena=%p has not been created", a);

                auto arena_type = arena_state_manip::get_type(a);
                HETCOMPUTE_INTERNAL_ASSERT(arena_type != NO_ARENA, "Unusable arena type");
                HETCOMPUTE_INTERNAL_ASSERT(_existing_arenas[arena_type] == a, "arena=%p does not exist in bufferstate=%p", a, this);

                HETCOMPUTE_INTERNAL_ASSERT(_valid_data_arenas[arena_type] == false || arena_state_manip::get_alloc_type(a) != UNALLOCATED,
                                         "arena=%p is marked as having valid data in bufstate=%p but is UNALLOCATED",
                                         a,
                                         this);

                return _valid_data_arenas[arena_type];
            }

            /// Returns true if the arena for arena_type has been created, been storage-allocated, and contains valid data.
            /// Returns false otherwise.
            bool is_valid_data_arena(arena_t arena_type) const
            {
                // Arena created?
                auto a = get(arena_type);
                if (a == nullptr)
                {
                    return false;
                }

                // Arena storage allocated? Sufficient to check validity.
                // - unallocated ==> _valid_data_arenas[arena_type] == false
                HETCOMPUTE_INTERNAL_ASSERT(_valid_data_arenas[arena_type] == false || arena_state_manip::get_alloc_type(a) != UNALLOCATED,
                                         "arena=%p is marked as having valid data in bufstate=%p but is UNALLOCATED",
                                         a,
                                         this);

                // Arena has valid data?
                return _valid_data_arenas[arena_type];
            }

            /// Adds a to _existing_arenas.
            ///  - a must not already be in _existing_arenas
            ///  - if has_valid_data = true ==>
            ///      + num_valid_data_arenas() must be 0
            ///      + a will also be marked valid in _valid_data_arenas
            void add_arena(arena* a, bool has_valid_data)
            {
                HETCOMPUTE_INTERNAL_ASSERT(a != nullptr, "Arena object needs to have been created a-priori by the bufferpolicy.");
                auto arena_type = arena_state_manip::get_type(a);
                HETCOMPUTE_INTERNAL_ASSERT(arena_type != NO_ARENA, "new arena does not have a usable arena type");
                HETCOMPUTE_INTERNAL_ASSERT(_existing_arenas[arena_type] == nullptr,
                                         "When adding arena=%p, arena=%p already present in this buffer "
                                         "for arena_type=%d bufferstate=%p",
                                         a,
                                         _existing_arenas[arena_type],
                                         static_cast<int>(arena_type),
                                         this);
                _existing_arenas[arena_type] = a;

                HETCOMPUTE_INTERNAL_ASSERT(has_valid_data == false || num_valid_data_arenas() == 0,
                                         "New arena brings valid data, but there are already arenas allocated with valid data.\n"
                                         "Disable this assertion if the buffer policy can guarantee that the new arena's data is"
                                         "already consistent.");
                if (has_valid_data)
                {
                    _valid_data_arenas[arena_type] = true;
                }
            }

            /// Sets the given arena as the unique arena with valid data.
            /// Other arenas are invalidated if present, but their data is
            /// first copied into unique_a if unique_a was not already valid.
            ///
            /// The calling context must ensure that if there are valid arenas
            /// present, and unique_a is currently not valid, then at least one of the
            /// other valid arenas will currently allow a data copy into unique_a, i.e.,
            ///    can_copy(other_a, unique_a) == true
            ///
            /// Bound arenas:
            /// If unique_a is bound to or bound by another arena, the
            /// other arena will still be marked as invalid even though it
            /// shares the same storage with unique_a. This strategy does
            /// not produce unnecesary copies as the arena-layer will optimize
            /// away the copy.
            void designate_as_unique_valid_data_arena(arena* unique_a)
            {
                HETCOMPUTE_INTERNAL_ASSERT(unique_a != nullptr, "Arena object needs to have been created a-priori by the bufferpolicy.");
                auto arena_type = arena_state_manip::get_type(unique_a);
                HETCOMPUTE_INTERNAL_ASSERT(arena_type != NO_ARENA, "Unusable arena type");
                HETCOMPUTE_INTERNAL_ASSERT(_existing_arenas[arena_type] == unique_a,
                                         "Arena to set as having valid data is not allocated in this buffer: unique_a=%p, bufferstate=%p\n",
                                         unique_a,
                                         this);
                if (_valid_data_arenas[arena_type] == false && buffer_holds_valid_data() == true)
                {
                    // there are other arenas with valid data
                    auto one_valid_arena = pick_optimal_valid_copy_from_arena(unique_a);
                    HETCOMPUTE_INTERNAL_ASSERT(!one_valid_arena.has_copy_conflict(),
                                             "Calling context should have ensured that data can be copied to mainmem_arena: bs=%p",
                                             this);
                    HETCOMPUTE_INTERNAL_ASSERT(one_valid_arena.src_arena() != nullptr, "Valid copy-from arena unexpectedly not found");
                    if (one_valid_arena.src_arena() != nullptr)
                    {   // to appease klocwork
                        copy_valid_data(one_valid_arena.src_arena(), unique_a);
                    }
                    else
                    {
                        HETCOMPUTE_FATAL("designate_as_unique_valid_data_arena(): Expected non-null value for one_valid_arena.src_arena().");
                    }
                }
                _valid_data_arenas[arena_type] = true;
                _existing_arenas[arena_type] = unique_a;

                HETCOMPUTE_INTERNAL_ASSERT(_existing_arenas.size() == _valid_data_arenas.size(),
                                         "_existing_arenas and _valid_data_arenas unexpectedly differ in size");
                // invalidate all the other arenas
                for (size_t i = 0; i < _existing_arenas.size(); i++)
                {
                    if (i != arena_type && _valid_data_arenas[i] == true)
                    {
                        invalidate_arena(_existing_arenas[i]);
                    }
                }

                // Post condition
                HETCOMPUTE_INTERNAL_ASSERT(_valid_data_arenas[arena_type] == true && num_valid_data_arenas() == 1,
                                         "designate_as_unique_valid_data_arena() must result in exactly one valid arena");
            }

            /// Invalidate the given arena if it has valid data.
            /// Caller is responsible for
            /// 1. a-priori saving any valid data, if there is a need to preserve the data.
            /// 2. ensuring that the arena can be invalidated, e.g., no task is currently accessing it.
            void invalidate_arena(arena* a)
            {
                HETCOMPUTE_INTERNAL_ASSERT(a != nullptr, "Arena is null");
                auto arena_type = arena_state_manip::get_type(a);
                if (_valid_data_arenas[arena_type] == true)
                {
                    arena_state_manip::invalidate(a);
                    _valid_data_arenas[arena_type] = false;
                }
            }

            /// Invalidate all arenas in the buffer, except for except_arena if != nullptr.
            /// Caller must ensure that all the arenas (except except_arena) can be invalidated,
            /// e.g., no task is currently accessing them.
            void invalidate_all_except(arena* except_arena = nullptr)
            {
                for (auto a : _existing_arenas)
                {
                    if (a != nullptr && a != except_arena)
                    {
                        invalidate_arena(a);
                    }
                }
            }

            /// Removes an arena from bufferstate (error if not present).
            /// Deletes the arena object if delete_arena = true (error if another arena is bound to a)
            /// Caller is responsible for
            /// 1. a-priori saving any valid data in 'a', if there is a need to preserve the data.
            /// 2. ensuring that the arena can be invalidated, e.g., no task is currently accessing it.
            void remove_arena(arena* a, bool delete_arena = false)
            {
                invalidate_arena(a);
                auto arena_type = arena_state_manip::get_type(a);
                HETCOMPUTE_INTERNAL_ASSERT(arena_type != NO_ARENA, "Unusable arena type");
                HETCOMPUTE_INTERNAL_ASSERT(_existing_arenas[arena_type] == a, "Removing arena=%p which is not present in bufferstate=%p", a, this);
                _existing_arenas[arena_type] = nullptr;
                if (delete_arena)
                {
                    for (auto other_arena : _existing_arenas)
                    {
                        if (other_arena != nullptr && arena_state_manip::get_bound_to_arena(other_arena) == a)
                        {
                            HETCOMPUTE_FATAL("Attempting to delete arena=%p, but another arena=%p is bound to it in bufstate=%s",
                                           a,
                                           other_arena,
                                           to_string().c_str());
                        }
                    }
                    arena_state_manip::delete_arena(a); // also deallocates arena's storage
                }
            }

            /// Finds the optimal valid from_arena to copy valid data from to the given
            /// to_arena, s.t., can_copy(from_arena, to_arena) == true.
            /// Optimality is in the sense of copy cost, with the chosen
            /// from_arena perhaps allowing zero-overhead copy to the given to_arena.
            ///
            /// Returns whether a copy conflict was detected and the optimal valid from_arena if found:
            ///  - When copy conflict detected, i.e., can_copy(..., to_arena) == false for every valid source arena
            ///      returns {true, nullptr}
            ///  - When no arena has valid data, i.e., num_valid_data_arenas() == 0
            ///      returns {false, nullptr}
            //   - If the to_arena is already valid, return the same
            //       returns {false, to_arena}
            ///  - Otherwise, a valid source arena has been found
            ///      returns {false, from_arena}
            ///
            /// @sa copy_from_arena_info on how to distinguish the return values
            copy_from_arena_info pick_optimal_valid_copy_from_arena(arena* to_arena) const
            {
                HETCOMPUTE_INTERNAL_ASSERT(to_arena != nullptr, "to_arena has not been created.");
                auto to_arena_type = arena_state_manip::get_type(to_arena);
                HETCOMPUTE_INTERNAL_ASSERT(to_arena_type != NO_ARENA, "Unusable arena type of to_arena=%p", to_arena);
                HETCOMPUTE_INTERNAL_ASSERT(_existing_arenas[to_arena_type] == to_arena,
                                         "to_arena=%p is not part of this bufferstate=%p",
                                         to_arena,
                                         this);
                if (_valid_data_arenas[to_arena_type] == true)
                {
                    return { false, to_arena }; // already valid, no need to copy from another arena
                }

                auto to_arena_alloc_type = arena_state_manip::get_alloc_type(to_arena);
                HETCOMPUTE_UNUSED(to_arena_alloc_type);
                // FIXME: need switch statement here on to_arena_alloc_type
                //  BOUND ==> return the arena bound-to if it's valid
                //  UNALLOCATED ==> lookup best valid arena for binding to (if any)
                //  INTERNAL, EXTERNAL ==> what valid arena allows minimum overhead copy?

                for (size_t i = 0; i < _existing_arenas.size(); i++)
                {
                    if (_existing_arenas[i] != nullptr && _valid_data_arenas[i] == true && _existing_arenas[i] != to_arena &&
                        can_copy(_existing_arenas[i], to_arena))
                    {
                        return { false, _existing_arenas[i] }; // valid source arena found
                    }
                }

                // Now either no valid arena exists in buffer or every valid source has copy conflicts. Distinguish.
                bool all_sources_conflict = buffer_holds_valid_data();
                return { all_sources_conflict, nullptr }; // no source arena found, either due to copy conflict or no valid data in buffer
            }

            /// Blocking transfer of data from one arena to another.
            /// Doesn't require that from_arena be considered to contain valid data.
            /// Doesn't mark the to_arena as having valid data after the copy.
            void copy_data(arena* from_arena, arena* to_arena)
            {
                HETCOMPUTE_INTERNAL_ASSERT(from_arena != nullptr && to_arena != nullptr,
                                         "from_arena=%p and to_arena=%p must both be created before copy",
                                         from_arena,
                                         to_arena);
                auto from_arena_type = arena_state_manip::get_type(from_arena);
                HETCOMPUTE_INTERNAL_ASSERT(from_arena_type != NO_ARENA, "Unusable arena type of from_arena=%p", from_arena);
                HETCOMPUTE_INTERNAL_ASSERT(_existing_arenas[from_arena_type] == from_arena,
                                         "from_arena=%p is not part of this bufferstate=%p",
                                         from_arena,
                                         this);
                auto to_arena_type = arena_state_manip::get_type(to_arena);
                HETCOMPUTE_INTERNAL_ASSERT(to_arena_type != NO_ARENA, "Unusable arena type of to_arena=%p", to_arena);
                HETCOMPUTE_INTERNAL_ASSERT(_existing_arenas[to_arena_type] == to_arena,
                                         "to_arena=%p is not part of this bufferstate=%p",
                                         to_arena,
                                         this);

                uint64_t copy_start_time = 0;
                if (_enable_buffer_statistics)
                {
                    copy_start_time = hetcompute_get_time_now();
                }

                copy_arenas(from_arena, to_arena);

                if (_enable_buffer_statistics)
                {
                    auto   duration    = hetcompute_get_time_now() - copy_start_time;
                    double duration_ms = duration / 1000000.0;

                    size_t i = from_arena_type;
                    size_t j = to_arena_type;

                    HETCOMPUTE_INTERNAL_ASSERT(_p_stats != nullptr, "_p_stats should have been allocated when statistics were enabled.");
                    _p_stats->_table[i][j].add_sample(duration_ms);
                }
            }

            /// Copies data from a valid from_arena. The to_arena is marked as valid.
            /// The calling context must ensure that the copy can succeed.
            void copy_valid_data(arena* valid_from_arena, arena* to_arena)
            {
                HETCOMPUTE_INTERNAL_ASSERT(valid_from_arena != nullptr, "valid_from_arena is not yet created");
                auto from_arena_type = arena_state_manip::get_type(valid_from_arena);
                HETCOMPUTE_INTERNAL_ASSERT(from_arena_type != NO_ARENA, "Unusuable arena type of valid_from_arena=%p", valid_from_arena);
                HETCOMPUTE_INTERNAL_ASSERT(_existing_arenas[from_arena_type] == valid_from_arena,
                                         "valid_from_arena=%p is not part of this bufferstate=%p",
                                         valid_from_arena,
                                         this);
                HETCOMPUTE_INTERNAL_ASSERT(_valid_data_arenas[from_arena_type] == true,
                                         "source arena=%p needs to have valid data",
                                         valid_from_arena);

                copy_data(valid_from_arena, to_arena);

                auto to_arena_type = arena_state_manip::get_type(to_arena);
                HETCOMPUTE_INTERNAL_ASSERT(to_arena_type != NO_ARENA, "Unusable arena type of to_arena=%p", to_arena);
                _valid_data_arenas[to_arena_type] = true;
            }

            /// Return value for add_acquire_requestor()
            /// - _no_conflict_found -- was the requestor in add_acquire_requestor() found in conflict with a prior requestor
            /// If _no_conflict_found == false, _conflicting_requestor identifies the already present conflicting requestor.
            /// Otherwise _conflicting_requestor = nullptr.
            /// If _no_conflict_found == false, _acquire_multiplicity gives acquire multiplicity of the requestor in add_acquire_requestor()
            /// Otherwise _acquire_multiplicity gives acquire multiplicity of _conflicting_requestor if
            ///    _conflicting_requestor != nullptr, o.w. =0.
            struct conflict_info
            {
                bool        _no_conflict_found;
                void const* _conflicting_requestor;
                size_t      _acquire_multiplicity;
            };

            /// Adds a buffer_acquire_info entry for a requestor, if compatible with existing requests.
            ///
            /// If no conflict found, returns with _no_conflict_found == true and _conflicting_requestor == nullptr.
            /// The requestor is added to the _acquire_set and the executor_device to device_hints. The arena
            /// accessed by that requestor can only be added later using update_acquire_info_with_arena().
            ///
            /// If a conflict is found, returns with _no_conflict_found == false. If the conflict is with a *confirmed*
            /// acquisition by a pre-existing request, _conflicting_requestor identifies the pre-existing conflicting requestor.
            /// Otherwise, _conflicting_requestor == nullptr to indicate that the conflict is with a pre-existing
            /// *tentative* request.
            ///
            /// The requestor is allowed to be already present in the _acquire_set. The new request by the same requestor
            /// needs to be compatible with the prior request for the new request to be granted. Granting of the new
            /// request increments the "acquire multiplicity" of the requestor in the _acquire_set.
            ///  - A subsequent acquire of an existing requestor with multiplicity must not be tentative, or the acquire will fail.
            ///  - A subsequent acquire of an existing requestor will fail if the prior existing acquire is tentative.
            ///
            /// Note1: a tentative request can be subsequently confirmed by invoking confirm_tentative_acquire_requestor()
            /// for that requestor.
            ///
            /// Note2: either a tentative request or a confirmed request can be removed using remove_acquire_requestor().
            ///
            /// Note3: get_multiplicity_for_acquire_requestor() queries the tentativeness and multiplicity for a requestor.
            ///
            /// Refer to buffer_acquire_set:
            /// buffer_acquire_set collects all the buffer arguments of a task and
            /// determines the superset access type for any repeated buffer arguments before
            /// calling add_acquire_requestor() only once for each unique buffer.
            /// buffer_acquire_set also atomically acquires all the buffer arguments
            /// for a task.
            conflict_info add_acquire_requestor(void const*                   requestor,
                                                executor_device_bitset        edb,
                                                buffer_acquire_info::access_t access,
                                                bool                          tentative_acquire)
            {
                HETCOMPUTE_INTERNAL_ASSERT(edb.count() > 0, "executor device doing acquire is unspecified");
                HETCOMPUTE_INTERNAL_ASSERT(access != buffer_acquire_info::unspecified, "Access type is unspecified during acquire");

                /*buffer_acquire_info lookup_acqinfo(requestor);
                HETCOMPUTE_INTERNAL_ASSERT(_acquire_set.find(lookup_acqinfo) == _acquire_set.end(),
                                         "requestor=%p is already present in acquire set of bufstate=%p",
                                         requestor,
                                         this);
                HETCOMPUTE_UNUSED(lookup_acqinfo);*/

                // Conflict identification heuristic:
                //  - We presently return the first conflict, even if the conflicting requestor is tentative.
                //  - It is possible to look for "better" conflicts -- first confirmed conflicting requestor if possible,
                //    otherwise, just the first tentative conflict. The benefit would be to eliminate spinning by requestor
                //    whenever a confirmed conficting requestor is already present (see: buffer_acquire_set, conversion
                //    of buffer conflicts into dynamic dependences).
                //  - The same requestor may already exist:
                //      + if the current access-type is compatible with the prior,
                //        we increment the requestor's acquire multiplicity and return success.
                //      + o.w., return conflict with itself..

                buffer_acquire_info lookup_acqinfo(requestor);
                auto                it = _acquire_set.find(lookup_acqinfo);

                // check if same requestor has already acquired, and update multiplicity
                if (it != _acquire_set.end())
                {
                    // same requestor exists

                    if (tentative_acquire)
                    { // is the current request tentative
                        HETCOMPUTE_DLOG("add_acquire_requestor() FAILED: requestor=%p cannot make a tentative request to acquire bufstate=%p "
                                      "with multiplicity (i.e., requestor has previously acquired buffer).",
                                      requestor,
                                      this);
                        return { false, it->_acquired_by, it->_acquire_multiplicity }; // requestor conflicts with itself
                    }

                    if (it->_tentative_acquire)
                    { // is the prior existing request tentative
                        HETCOMPUTE_DLOG("add_acquire_requestor() FAILED: requestor=%p has already acquired bufstate=%p tentatively. "
                                      "Cannot acquire with multiplicity till confirmation or release of prior tentative request",
                                      requestor,
                                      this);
                        return { false, it->_acquired_by, it->_acquire_multiplicity }; // requestor conflicts with itself
                    }

                    // now, prior instance of requestor is a confirmed acquire, and the new request is not tentative

                    if (it->_access == buffer_acquire_info::read && it->_access != access)
                    {
                        return { false, it->_acquired_by, it->_acquire_multiplicity }; // incompatible access
                    }

                    // no incompatibility found
                    auto entry = *it;
                    entry._acquire_multiplicity++;
                    _acquire_set.erase(entry);
                    _acquire_set.insert(entry);
                    return { true, nullptr, entry._acquire_multiplicity }; // successfully updated multiplicity
                }

                // check compatibility against other existing acquirers
                if (access == buffer_acquire_info::read)
                {
                    for (auto const& acreq : _acquire_set)
                    {
                        if (acreq._access != buffer_acquire_info::read)
                        {
                            auto confirmed_conflicting_requestor = (acreq._tentative_acquire ? nullptr : acreq._acquired_by);
                            auto acquire_multiplicity            = acreq._acquire_multiplicity;
                            HETCOMPUTE_INTERNAL_ASSERT((confirmed_conflicting_requestor == nullptr) xor (acquire_multiplicity > 0),
                                                     "Only non-null confirmed_conflicting_requestor should have non-zero acquire "
                                                     "multiplicity");
                            return { false, confirmed_conflicting_requestor, acquire_multiplicity }; // only other readers allowed
                        }
                    }
                }
                else
                {
                    if (!_acquire_set.empty())
                    {
                        auto const& first_acreq                     = *(_acquire_set.begin());
                        auto        confirmed_conflicting_requestor = (first_acreq._tentative_acquire ? nullptr : first_acreq._acquired_by);
                        auto        acquire_multiplicity            = first_acreq._acquire_multiplicity;
                        HETCOMPUTE_INTERNAL_ASSERT((confirmed_conflicting_requestor == nullptr) xor (acquire_multiplicity > 0),
                                                 "Only non-null confirmed_conflicting_requestor should have non-zero acquire multiplicity");
                        return { false, confirmed_conflicting_requestor, acquire_multiplicity }; // writer must be exclusive (for now)
                    }
                }

                // NOTE: acquired arenas will be filled later by update_acquire_info_with_arena()
                size_t acquire_multiplicity = (tentative_acquire ? 0 : 1); // multiplicity only tracks confirmed acquires
                _acquire_set.insert(buffer_acquire_info(requestor, edb, access, tentative_acquire, acquire_multiplicity));

                edb.for_each([&](executor_device ed) {
                    switch (ed)
                    {
                    default:
                    case executor_device::unspecified:
                        HETCOMPUTE_UNREACHABLE("Unspecified executor_device");
                        break;
                    case executor_device::cpu:
                        this->_device_hints.add(device_type::cpu);
                        break;
                    case executor_device::gpucl:
                    case executor_device::gpugl:
                        this->_device_hints.add(device_type::gpu);
                        break;
                    case executor_device::gputexture:
                        break; // textures used entirely on demand, no upfront optimizations needed
                    case executor_device::dsp:
                        this->_device_hints.add(device_type::dsp);
                        break;
                    }
                });

                return { true, nullptr, acquire_multiplicity }; // successfully added, no conflict found
            }

            /// Confirms a previously tentative requestor.
            ///
            /// Error if the requestor is not already present in _acquire_set as tentative.
            void confirm_tentative_acquire_requestor(void const* requestor)
            {
                buffer_acquire_info lookup_acqinfo(requestor);
                auto                it = _acquire_set.find(lookup_acqinfo);
                HETCOMPUTE_INTERNAL_ASSERT(it != _acquire_set.end(), "requestor=%p not found in bufstate=%p", requestor, this);
                auto entry = *it;
                HETCOMPUTE_INTERNAL_ASSERT(entry._tentative_acquire == true,
                                         "Entry for requestor=%p was not tentative in bufstate=%p",
                                         requestor,
                                         this);

                HETCOMPUTE_INTERNAL_ASSERT(entry._acquire_multiplicity == 0,
                                         "Multiplicity should be 0 for a tentative acquire: for requestor=%p in bufstate=%p",
                                         requestor,
                                         this);

                entry._tentative_acquire    = false;
                entry._acquire_multiplicity = 1;
                _acquire_set.erase(entry);
                _acquire_set.insert(entry);
            }

            void update_acquire_info_with_arena(void const* requestor, executor_device ed, arena* acquired_arena)
            {
                HETCOMPUTE_INTERNAL_ASSERT(acquired_arena != nullptr, "Error. acquired_arena is null");

                buffer_acquire_info lookup_acqinfo(requestor);
                auto                it = _acquire_set.find(lookup_acqinfo);
                HETCOMPUTE_INTERNAL_ASSERT(it != _acquire_set.end(), "buffer_acquire_info not found: %s", lookup_acqinfo.to_string().c_str());
                auto entry = *it;

                auto ied = static_cast<size_t>(ed);
                HETCOMPUTE_INTERNAL_ASSERT(ied >= static_cast<size_t>(executor_device::first) &&
                                             ied <= static_cast<size_t>(executor_device::last),
                                         "Invalid executor device=%zu",
                                         ied);
                entry._acquired_arena_per_device[ied] = acquired_arena;

                _acquire_set.erase(entry);
                _acquire_set.insert(entry);
            }

            buffer_acquire_info::access_t get_access_type_for_acquire_requestor(void const* requestor)
            {
                buffer_acquire_info lookup_acqinfo(requestor);
                auto                it = _acquire_set.find(lookup_acqinfo);
                if (it == _acquire_set.end())
                {
                    return buffer_acquire_info::unspecified; // requestor has not acquired the buffer
                }

                return it->_access;
            }

            /// Queries the acquire-type (tentative or not) and the multiplicity of a requestor.
            /// Returns {false, 0} -- requestor has not acquired the buffer
            ///         {true,  0} -- requestor has acquired the buffer tentatively
            ///         {false, n} -- requestor has acquired the buffer with multiplicity n (n > 0)
            std::pair<bool, size_t> get_multiplicity_for_acquire_requestor(void const* requestor)
            {
                buffer_acquire_info lookup_acqinfo(requestor);
                auto                it = _acquire_set.find(lookup_acqinfo);
                if (it == _acquire_set.end())
                    return { false, 0 }; // requestor has not acquired the buffer

                // requestor exists
                HETCOMPUTE_INTERNAL_ASSERT(it->_tentative_acquire xor (it->_acquire_multiplicity > 0),
                                         "requestor=%p can either be tentative or have multiplicity",
                                         requestor);
                return { it->_tentative_acquire, it->_acquire_multiplicity };
            }

            /// Deletes the requestor from the acquire set if tentative or confirmed multiplicity == 1.
            /// Decrements confirmed multiplicity if > 1. Returns remaining acquire multiplicity.
            /// Error if the requestor is not present.
            /// Also releases arenas acquired for the requestor for each device.
            size_t remove_acquire_requestor(void const* requestor)
            {
                buffer_acquire_info lookup_acqinfo(requestor);
                auto                it = _acquire_set.find(lookup_acqinfo);
                HETCOMPUTE_INTERNAL_ASSERT(it != _acquire_set.end(), "buffer_acquire_info not found: %s", lookup_acqinfo.to_string().c_str());

                auto entry = *it;
                if (it->_tentative_acquire)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(it->_acquire_multiplicity == 0,
                                             "tentative acquire should have 0 acquire multiplicity, found %zu for requestor=%p in "
                                             "bufstate=%p",
                                             it->_acquire_multiplicity,
                                             requestor,
                                             this);
                    it->_edb.for_each([&](executor_device ed) {
                        auto ied = static_cast<size_t>(ed);
                        HETCOMPUTE_UNUSED(ied);
                        HETCOMPUTE_INTERNAL_ASSERT(it->_acquired_arena_per_device[ied] == nullptr,
                                                 "A tentative requestor=%p was found with non-null arena=%p for ed=%zu in bufstate=%p",
                                                 requestor,
                                                 it->_acquired_arena_per_device[ied],
                                                 ied,
                                                 this);
                    });
                }
                else
                { // confirmed acquire
                    HETCOMPUTE_INTERNAL_ASSERT(it->_acquire_multiplicity > 0,
                                             "confirmed acquire should have > 0 acquire multiplicity, found 0 for requestor=%p in "
                                             "bufstate=%p",
                                             requestor,
                                             this);
                    if (it->_acquire_multiplicity == 1)
                    { // is this the last multiplicity of this requestor?
                        it->_edb.for_each([&](executor_device ed) {
                            auto ied = static_cast<size_t>(ed);
                            HETCOMPUTE_INTERNAL_ASSERT(it->_acquired_arena_per_device[ied] != nullptr,
                                                     "Acquired arena is nullptr for ed=%zu for confirmed requestor=%p in bufstate=%p",
                                                     ied,
                                                     requestor,
                                                     this);
                            arena_state_manip::unref(it->_acquired_arena_per_device[ied]);
                        });
                    }
                    entry._acquire_multiplicity--;
                }

                _acquire_set.erase(*it);

                HETCOMPUTE_INTERNAL_ASSERT(_acquire_set.find(lookup_acqinfo) == _acquire_set.end(),
                                         "Multiple entries found for requestor=%p in bufstate=%p",
                                         requestor,
                                         this);

                if (entry._acquire_multiplicity > 0)
                {                               // not actually released
                    _acquire_set.insert(entry); // re-insert with decremented acquire multiplicity
                }
                else
                { // actually released
                    if (_pending_host_acquires)
                    {
                        _pending_host_acquires = false;
                        _cv.notify_all(); // re-tries will be stalled until _mutex is finally released by the current releasing context
                    }
                }

                return entry._acquire_multiplicity;
            }

            /// Blocks a host acquire until signalled by the release of the buffer by another requestor.
            /// Invariant: must be called with lock holding _mutex locked.
            void wait_for_release_signal(std::unique_lock<std::mutex>& lock)
            {
                _pending_host_acquires = true;
                _cv.wait(lock);
            }

            /// Return any confirmed acquire requestor.
            /// Returns nullptr if none found.
            void const* get_any_confirmed_acquire_requestor() const
            {
                for (auto const& bai : _acquire_set)
                {
                    if (!bai._tentative_acquire)
                    {
                        return bai._acquired_by;
                    }
                }
                return nullptr;
            }

            /// Determines the overall access type for an arena within a buffer
            /// across all requestors.
            /// Implementation: the search can stop at the first requestor using the arena,
            ///    as multiple requestors can occur only when all have read-only access.
            ///
            /// TODO: This API will no longer be needed once host code uses acquire-release APIs.
            buffer_acquire_info::access_t get_superset_access_type_for_arena(arena* query_a) const
            {
                HETCOMPUTE_INTERNAL_ASSERT(query_a != nullptr, "Need a non-null query arena");

                // Implemented as a linear-search. May need better data structures if _acquire_set
                // is likely to be large (i.e., if there are many concurrent acquirers)
                for (auto const& bai : _acquire_set)
                {
                    for (auto acq_a : bai._acquired_arena_per_device)
                    {
                        if (acq_a == query_a)
                        {
                            HETCOMPUTE_INTERNAL_ASSERT(bai._access != buffer_acquire_info::unspecified,
                                                     "requestor=%p has unspecified access type for bufstate=%p",
                                                     bai._acquired_by,
                                                     this);
                            // Assumption: If query_a occurs in multiple bai, the permission
                            // must be buffer_acquire_info::read, o.w., there would be at most one bai.
                            return bai._access;
                        }
                    }
                }
                return buffer_acquire_info::unspecified;
            }

            /// Returns buffer_acquire_info::unspecified if no requestors present.
            /// Returns the overall access type of the buffer if at least one
            /// requestor present. The overall access type will be the same as
            /// the first requestor's access type because:
            ///  - multiple requestors can be present only if they are all readers.
            ///  - a write or readwrite requestor must be exclusive (i.e. not oher requestors present)
            buffer_acquire_info::access_t get_access_type() const
            {
                if (_acquire_set.empty())
                {
                    return buffer_acquire_info::unspecified;
                }
                return _acquire_set.begin()->_access;
            }

            /// String representation of arenas in bufferstate in the following format:
            ///   [arena_info arena_info ...] for the types of possible arena,
            ///
            /// where each arena_info has the following format:
            ///   arena-type-enum-value = (arena-pointer/nullptr data-valid/invalid unbound/bound-to-which-arena-type-enum-value)
            std::string to_string() const
            {
                std::string s("");
                if (_name != "")
                {
                    s.append("---" + _name + "--- ");
                }
                s.append("[");
                for (size_t i = 0; i < _existing_arenas.size(); i++)
                {
                    auto a              = _existing_arenas[i];
                    auto bound_to_arena = (a == nullptr ? nullptr : arena_state_manip::get_bound_to_arena(a));

                    size_t index_of_bound_to_arena = _existing_arenas.size(); // initialize to invalid value
                    if (bound_to_arena != nullptr)
                    {
                        for (size_t j = 0; j < _existing_arenas.size(); j++)
                        {
                            if (_existing_arenas[j] == bound_to_arena)
                            {
                                index_of_bound_to_arena = j;
                                break;
                            }
                        }
                    }

                    s.append(::hetcompute::internal::strprintf("%zu=(%p %s %s) ",
                                                             i,
                                                             _existing_arenas[i],
                                                             _valid_data_arenas[i] ? "V" : "I",
                                                             (bound_to_arena == nullptr ?
                                                                  "UB" :
                                                                  ::hetcompute::internal::strprintf("B%zu", index_of_bound_to_arena).c_str())));
                }
                s += "]";
                return s;
            }

            /// Assigns a name to the buffer. Useful for tracking activity on a buffer
            /// during debugging.
            void set_name(std::string const& name)
            {
                _name = name;
            }

            /// Start / stop (including resume / restart) collection of statistics for the buffer
            void collect_statistics(bool enable, bool print_at_buffer_dealloc = true)
            {
                if (_p_stats == nullptr)
                {
                    _p_stats.reset(new buffer_statistics);
                }

                _enable_buffer_statistics    = enable;
                _print_statistics_at_dealloc = print_at_buffer_dealloc;
            }

            std::string statistics_to_string() const
            {
                std::string s("");
                if (_name != "")
                {
                    s.append("---" + _name + "---");
                }
                s.append("\n");

                if (_p_stats == nullptr)
                {
                    return s;
                }

                for (size_t i = 0; i < _p_stats->_table.size(); i++)
                {
                    s.append(::hetcompute::internal::strprintf("%zu: [", i));
                    for (size_t j = 0; j < _p_stats->_table[i].size(); j++)
                    {
                        auto& e = _p_stats->_table[i][j];
                        s.append(
                            ::hetcompute::internal::strprintf("%zu (%zu, %lf, %lf) ", j, e.get_count(), e.get_mean(), std::sqrt(e.get_var())));
                    }
                    s.append("]\n");
                }
                return s;
            }

            /// Get arena-copy statistics between a specified src and dest arena.
            /// May only be invoked after collect_statistics() has been invoked on the buffer.
            buffer_statistics::arena_pair_copy_stats get_stats(arena_t from_arena_type, arena_t to_arena_type) const
            {
                HETCOMPUTE_API_ASSERT(_p_stats != nullptr, "Please ensure collect_statistics() is called before get_stats()");
                return _p_stats->_table[from_arena_type][to_arena_type];
            }
        };  // class bufferstate
    };  // namespace internal
};  // namespace hetcompute
