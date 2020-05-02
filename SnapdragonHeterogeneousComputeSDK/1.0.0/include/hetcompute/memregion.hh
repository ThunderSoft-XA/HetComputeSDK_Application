#pragma once

#include <hetcompute/internal/buffer/memregion-internal.hh>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/macros.hh>

namespace hetcompute
{
    namespace internal
    {
        // Forward declaration
        class memregion_base_accessor;
    };  // namespace internal

    /** @addtogroup memregion_doc
    @{ */

    /**
     *  @brief Base class for all mem-regions.
     *
     *  Base class for all mem-regions. The only common feature
     *  across the mem-regions is that they all have a size in bytes.
     *  The user constructs a mem-region of the appropriate type
     *  to allocate the corresponding type of specialized device-memory
     *  (<code>hetcompute::main_memregion</code> and <code>hetcompute::ion_memregion</code>)
     *  or to create inter-operability with data from an external framework
     *  (<code>hetcompute::glbuffer_memregion</code>).
     *
     *  Mem-regions provide RAII semantics:
     *   - The specialized memory is allocated or the interop created when the
     *     user constructs the mem-region object of the appropriate type.
     *   - The user keeps the allocated memory or the interop alive by
     *     keeping the mem-region object alive.
     *
     *  @note The base class <code>hetcompute::memregion</code> is not user-constructible.
     *        The user may construct from a derived class of <code>hetcompute::memregion</code>
     *        that provides the desired allocation or interop functionaity.
     *
     *  @sa hetcompute::main_memregion
     *  @sa hetcompute::cl2svm_memregion
     *  @sa hetcompute::ion_memregion
     *  @sa hetcompute::glbuffer_memregion
     */
    class memregion
    {
        friend class ::hetcompute::internal::memregion_base_accessor;

    protected:
        explicit memregion(internal::internal_memregion* int_mr) : _int_mr(int_mr)
        {
        }

        internal::internal_memregion* _int_mr;

    public:
        /**
         * @brief Destructor
         */
        ~memregion()
        {
            HETCOMPUTE_INTERNAL_ASSERT(_int_mr != nullptr, "Error. Internal memregion is null");
            delete _int_mr;
        }

        /**
         *  @brief Get the size of the mem-region in bytes.
         *
         *  Get the size of the mem-region in bytes. Applies to all
         *  derived mem-region classes.
         *
         *  @return Size of the mem-region in bytes.
         */
        size_t get_num_bytes() const
        {
            return _int_mr->get_num_bytes();
        }

        HETCOMPUTE_DELETE_METHOD(memregion());
        HETCOMPUTE_DELETE_METHOD(memregion(memregion const&));
        HETCOMPUTE_DELETE_METHOD(memregion& operator=(memregion const&));
        HETCOMPUTE_DELETE_METHOD(memregion(memregion&&));
        HETCOMPUTE_DELETE_METHOD(memregion& operator=(memregion&&));
    };  // class memregion

    /** @} */ /* end_addtogroup memregion_doc */

    /** @addtogroup memregion_doc
    @{ */

    /**
     *  @brief Allocates aligned memory from the platform main memory.
     *
     *  Allocates aligned memory from the platform main memory.
     *  The default alignment is 4096 bytes to get page-aligned allocation.
     *  A derived class of <code>hetcompute::memregion</code>.
     *
     *  @sa hetcompute::memregion
    */
    class main_memregion : public memregion
    {
    public:
        /**
         *  @brief The default alignment needed for the allocation to be page aligned.
         *
         *  The default alignment needed for the allocation to be page aligned.
         */
        static constexpr size_t s_default_alignment = 4096;

        /**
         *  @brief Constructor, allocates aligned memory.
         *
         *  Constructor, allocates aligned memory.
         *
         *  @param sz        Size of the allocation in bytes.
         *
         *  @param alignment <em>Optional</em>, desired alignment. Default is page aligned.
         */
        explicit main_memregion(size_t sz, size_t alignment = s_default_alignment)
            : memregion(new internal::internal_main_memregion(sz, alignment))
        {
        }

        /**
       *  @brief Constructor, uses user-allocated memory.
       *
       *  Constructor, uses user-allocated memory.
       *  The user is responsible for ensuring the lifetime of the memory,
       *  and handling the deallocation. The lifetime of the user allocated
       *  memory MUST exceed any use of the memory via the memregion object (say,
       *  if the memregion is used by a buffer).
       *
       *  @param ptr       Pointer to the externally allocated region.
       *
       *  @param sz        Size of the allocation in bytes.
       */
        main_memregion(void* ptr, size_t sz) : memregion(new internal::internal_main_memregion(ptr, sz))
        {
        }

        HETCOMPUTE_DELETE_METHOD(main_memregion());
        HETCOMPUTE_DELETE_METHOD(main_memregion(main_memregion const&));
        HETCOMPUTE_DELETE_METHOD(main_memregion& operator=(main_memregion const&));
        HETCOMPUTE_DELETE_METHOD(main_memregion(main_memregion&&));
        HETCOMPUTE_DELETE_METHOD(main_memregion& operator=(main_memregion&&));

        /**
         *  @brief Gets a pointer to the allocated memory.
         *
         *  Gets a pointer to the allocated memory.
         *
         *  @return A pointer to the allocated memory.
         */
        void* get_ptr() const
        {
            return _int_mr->get_ptr();
        }
    };  // class main_memregion

#ifdef HETCOMPUTE_HAVE_QTI_DSP
    /**
     *  @brief Allocates ION memory on platforms that support it.
     *
     *  Allocates ION memory on platforms that support it.
     *  The ION memory can be allocated as cacheable or non-cacheable.
     *  A derived class of <code>hetcompute::memregion</code>.
     *
     *  @sa hetcompute::memregion
     */
    class ion_memregion : public memregion
    {
    public:
        /**
         *  @brief Constructor, allocates ION memory.
         *
         *  Constructor, allocates ION memory.
         *
         *  @param sz        Size of the allocation in bytes.
         *
         *  @param cacheable <em>Optional</em>, cacheable if <code>true</code>,
         *                   non-cacheable if <code>false</code>. Default is cacheable.
         */
        explicit ion_memregion(size_t sz, bool cacheable = true) : memregion(new internal::internal_ion_memregion(sz, cacheable))
        {
        }

        /**
       *  @brief Constructor, uses allocated ION memory.
       *
       *  Constructor, uses allocated ION memory.
       *  The user is responsible for ensuring the lifetime of the ION memory,
       *  and handling the deallocation. The lifetime of the user allocated
       *  memory MUST exceed any use of the memory via the memregion object (say,
       *  if the memregion is used by a buffer).
       *
       *  @param ptr       Pointer to the externally allocated region.The block at ptr
       *                   of size sz bytes must be fully contained within an existing
       *                   HetCompute ion_memregion
       *
       *  @param sz        Size of the allocation in bytes.
       *
       *  @param cacheable <code>true</code> if ptr points to a cacheable ion region,
       *                   <code>false</code> otherwise.
       */
        ion_memregion(void* ptr, size_t sz, bool cacheable) : memregion(new internal::internal_ion_memregion(ptr, sz, cacheable))
        {
        }

        /**
         *  @brief Constructor, uses allocated ION memory, with the associated file descriptor.
         *
         *  Constructor, uses allocated ION memory, with the associated file descriptor.
         *  The user is responsible for ensuring the lifetime of the ION memory,
         *  and handling the deallocation. The lifetime of the user allocated
         *  memory MUST exceed any use of the memory via the memregion object (say,
         *  if the memregion is used by a buffer). This variant is more flexible as it
         *  enables the construction of an ion_memregion using ion memory that was
         *  (a) allocated by another process, or, (b) allocated by the same process
         *  without using a hetcompute::ion_memregion.
         *
         *  @param ptr       Pointer to the externally allocated region.
         *
         *  @param fd        File descriptor associated with the allocated ion pointer.
         *
         *  @param sz        Size of the allocation in bytes.
         *
         *  @param cacheable <code>true</code> if ptr points to a cacheable ion region,
         *                   <code>false</code> otherwise.
         */
        ion_memregion(void* ptr, int fd, size_t sz, bool cacheable)
            : memregion(new internal::internal_ion_memregion(ptr, fd, sz, cacheable))
        {
        }

        HETCOMPUTE_DELETE_METHOD(ion_memregion());
        HETCOMPUTE_DELETE_METHOD(ion_memregion(ion_memregion const&));
        HETCOMPUTE_DELETE_METHOD(ion_memregion& operator=(ion_memregion const&));
        HETCOMPUTE_DELETE_METHOD(ion_memregion(ion_memregion&&));
        HETCOMPUTE_DELETE_METHOD(ion_memregion& operator=(ion_memregion&&));

        /**
         *  @brief Gets a pointer to the allocated ION memory.
         *
         *  Gets a pointer to the allocated ION memory.
         *
         *  @return A pointer to the allocated ION memory.
         */
        void* get_ptr() const
        {
            return _int_mr->get_ptr();
        }

        /**
         *  @brief Gets the file descriptor associated with the pointer to the allocated ION memory.
         *
         *  Gets the file descriptor associated with the pointer to the allocated ION memory.
         *
         *  @return The file descriptor associated with the allocated ION memory.
         */
        int get_fd() const
        {
            return _int_mr->get_fd();
        }

        /**
         *  @brief Returns whether the ION memregion is cacheable.
         *
         *  Returns whether the ION memregion is cacheable.
         *
         *  @return whether the ION memregion is cacheable.
         */
        bool is_cacheable() const
        {
            return _int_mr->is_cacheable();
        }
    };  // class ion_memregion
#endif // HETCOMPUTE_HAVE_QTI_DSP

#ifdef HETCOMPUTE_HAVE_GLES
    /**
     *  @brief Creates inter-operability with an OpenGL buffer.
     *
     *  Creates inter-operability with an OpenGL buffer.
     *  The user may have an external OpenGL buffer. Qualcomm HetCompute may access the OpenGL buffer
     *  once inter-operability has been setup using an instance of this class.
     *  A derived class of <code>hetcompute::memregion</code>.
     *
     *  @sa hetcompute::memregion
     */
    class glbuffer_memregion : public memregion
    {
    public:
        /**
         *  @brief Constructor, wraps an existing OpenGL buffer to allow inter-operability
         *  with Qualcomm HetCompute.
         *
         *  Constructor, wraps an existing OpenGL buffer to allow inter-operability
         *  with Qualcomm HetCompute. The size of the OpenGL buffer is automatically determined
         *  and set as the size of the mem-region.
         *
         *  @param id  The <code>GLuint</code> id of an existing OpenGL buffer.
         */
        explicit glbuffer_memregion(GLuint id) : memregion(new internal::internal_glbuffer_memregion(id))
        {
        }

        HETCOMPUTE_DELETE_METHOD(glbuffer_memregion());
        HETCOMPUTE_DELETE_METHOD(glbuffer_memregion(glbuffer_memregion const&));
        HETCOMPUTE_DELETE_METHOD(glbuffer_memregion& operator=(glbuffer_memregion const&));
        HETCOMPUTE_DELETE_METHOD(glbuffer_memregion(glbuffer_memregion&&));
        HETCOMPUTE_DELETE_METHOD(glbuffer_memregion& operator=(glbuffer_memregion&&));

        /**
         *  @brief Gets the id of the wrapped OpenGL buffer.
         *
         *  Gets the id of the wrapped OpenGL buffer.
         *
         *  @return The id of the wrapped OpenGL buffer.
         */
        GLuint get_id() const
        {
            return _int_mr->get_id();
        }
    };
#endif // HETCOMPUTE_HAVE_GLES

};  // namespace hetcompute
