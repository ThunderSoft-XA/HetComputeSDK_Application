#pragma once

#include <hetcompute/internal/buffer/executordevice.hh>
#include <hetcompute/internal/device/gpuopencl.hh>
#include <hetcompute/internal/device/gpuopengl.hh>

namespace hetcompute
{
    namespace graphics
    {
        namespace internal
        {
#ifdef HETCOMPUTE_HAVE_OPENCL
            // Forward declarations
            class base_texture_cl;
#endif // HETCOMPUTE_HAVE_OPENCL
        }; // namespace internal
    };  // namespace graphics
};  // namespace hetcompute

namespace hetcompute
{
    namespace internal
    {
        enum arena_t : size_t
        {
            MAINMEM_ARENA = 0,
#ifdef HETCOMPUTE_HAVE_OPENCL
            CL_ARENA = 1,
#endif // HETCOMPUTE_HAVE_OPENCL
#ifdef HETCOMPUTE_HAVE_QTI_DSP
            ION_ARENA = 2,
#endif // HETCOMPUTE_HAVE_QTI_DSP
#ifdef HETCOMPUTE_HAVE_GLES
            GL_ARENA = 3,
#endif // HETCOMPUTE_HAVE_GLES
#ifdef HETCOMPUTE_HAVE_OPENCL
            TEXTURE_ARENA = 4,
#endif // HETCOMPUTE_HAVE_OPENCL
            NO_ARENA = 5
        };

        constexpr size_t NUM_ARENA_TYPES = arena_t::NO_ARENA;

        enum arena_alloc_t
        {
            UNALLOCATED,
            INTERNAL,
            EXTERNAL,
            BOUND
        };

        // Forward declarations
        class arena;
        class mainmem_arena;
#ifdef HETCOMPUTE_HAVE_OPENCL
        class cl_arena;
#endif // HETCOMPUTE_HAVE_OPENCL
#ifdef HETCOMPUTE_HAVE_QTI_DSP
        class ion_arena;
#endif // HETCOMPUTE_HAVE_QTI_DSP
#ifdef HETCOMPUTE_HAVE_GLES
        class gl_arena;
#endif // HETCOMPUTE_HAVE_GLES
#ifdef HETCOMPUTE_HAVE_OPENCL
        class texture_arena;
#endif // HETCOMPUTE_HAVE_OPENCL

        bool can_copy(arena* src, arena* dest);
        void copy_arenas(arena* src, arena* dest);

        /// Accessor to avoid exposing the full arena API,
        /// specially the methods whose signatures rely on implementation details.
        struct arena_state_manip
        {
            /// Caller must ensure no task or host code is accessing arena.
            static void delete_arena(arena* a);

            /// Can be called anytime for a non-null arena.
            static arena_t get_type(arena* a);

            /// Can be called anytime once the arena alloc type is no longer
            /// UNALLOCATED (as it will not change).
            /// Otherwise, caller must ensure there is no concurrent access
            /// to arena that may alter the allocation-type, such as copying
            /// of data between arenas.
            static arena_alloc_t get_alloc_type(arena* a);

            /// Caller must ensure no task or host code is accessing arena.
            static void invalidate(arena* a);

            /// Can be called anytime once the arena alloc type is no longer
            /// UNALLOCATED (as it will not change).
            /// Otherwise, caller must ensure there is no concurrent access
            /// to arena that may alter the allocation-type, such as copying
            /// of data between arenas.
            static arena* get_bound_to_arena(arena* a);

            /// Caller must ensure there is no concurrent access to arena state.
            static void ref(arena* a);

            /// Caller must ensure there is no concurrent access to arena state.
            static void unref(arena* a);

            /// Caller must ensure there is no concurrent access to arena state.
            static size_t get_ref_count(arena* a);
        };  // struct arena_state_manip

        struct arena_storage_accessor
        {
            static void* access_mainmem_arena_for_cputask(arena* acquired_arena);

#ifdef HETCOMPUTE_HAVE_OPENCL
            static cl::Buffer& access_cl_arena_for_gputask(arena* acquired_arena);
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_GLES
            static GLuint access_gl_arena_for_gputask(arena* acquired_arena);
#endif // HETCOMPUTE_HAVE_GLES

#ifdef HETCOMPUTE_HAVE_OPENCL
            static hetcompute::graphics::internal::base_texture_cl* access_texture_arena_for_gputask(arena* acquired_arena);
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_QTI_DSP
            static void* access_ion_arena_for_dsptask(arena* acquired_arena);
#endif // HETCOMPUTE_HAVE_QTI_DSP

#ifdef HETCOMPUTE_HAVE_OPENCL
            static bool texture_arena_has_format(arena* a);
#endif // HETCOMPUTE_HAVE_OPENCL

            static void* get_ptr(arena* a);
        };  // struct arena_storage_accessor

    }; // namespace internal
};     // namespace hetcompute
