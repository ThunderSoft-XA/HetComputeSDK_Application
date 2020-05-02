/** @file buffer.hh */
#pragma once

#include <cstddef>
#include <string>

#include <hetcompute/texturetype.hh>
#include <hetcompute/bufferiterator.hh>
#include <hetcompute/devicetypes.hh>
#include <hetcompute/memregion.hh>

#include <hetcompute/internal/buffer/buffer-internal.hh>
#include <hetcompute/internal/compat/compiler_compat.h>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/strprintf.hh>

namespace hetcompute
{
    template <typename T>
    buffer_ptr<T> create_buffer(size_t num_elems)
    {
        return create_buffer<T>(num_elems, device_set());
    }

    /** @addtogroup buffer_doc
        @{ */

    /**
     *  @brief Creates a buffer of datatype <code>T</code> of the requested size.
     *
     *  Creates a buffer of datatype <code>T</code> of the requested size.
     *  Fatal error if <code>num_elems</code> is 0.
     *
     *  @tparam T               User data type for buffer.
     *
     *  @param  num_elems       Number of elements of type <code>T</code> in buffer.
     *                          Must be larger than zero.
     *  @param  likely_devices  <em>Optional</em>, default is an empty
     *                          <code>hetcompute::device_set</code>. Allows the programmer
     *                          to convey advance knowledge of which device-types
     *                          may access this buffer, thereby allowing Qualcomm HetCompute to
     *                          internally determine an optimal storage and
     *                          data-transfer policy for this buffer. This information
     *                          is only used as a hint to guide internal storage-allocation
     *                          and data-transfer optimizations, and is allowed to be
     *                          partial or incorrect.
     *
     *  @return A buffer pointer to the created buffer.
     *
     *  The optional parameters allow for the following variants to this call.
     *  @code
     *  hetcompute::create_buffer(size_t num_elems);
     *
     *  hetcompute::create_buffer(size_t num_elems,
     *                          hetcompute::device_set const& likely_devices);
     *  @endcode
     */
    template <typename T>
    buffer_ptr<T> create_buffer(size_t num_elems, device_set const& likely_devices);
    /** @} */ /* end_addtogroup buffer_doc */

    template <typename T>
    buffer_ptr<T> create_buffer(T* preallocated_ptr, size_t num_elems)
    {
        return create_buffer<T>(preallocated_ptr, num_elems, device_set());
    }  

    /** @addtogroup buffer_doc
        @{ */

    /**
     *  @brief Creates a buffer of datatype <code>T</code> of the requested size
     *         from a pre-allocated pointer.
     *
     *  Creates a buffer of datatype <code>T</code> of the requested size
     *  from a pre-allocated pointer.
     *  The pre-allocated pointer provides initial storage and potentially
     *  initial data for the buffer.
     *  Fatal error if <code>num_elems</code> is 0.
     *
     *  @tparam T               User data type for buffer.
     *
     *  @param  preallocated_ptr Pointer to pre-allocated contiguous memory of size
     *                           at least <code>num_elems * sizeof(T)</code> bytes.
     *
     *  @param  num_elems        Number of elements of type <code>T</code> in buffer.
     *                           Must be larger than zero.
     *
     *  @param  likely_devices  <em>Optional</em>, default is an empty
     *                          <code>hetcompute::device_set</code>. Allows the programmer to
     *                          convey advance knowledge of which device-types
     *                          may access this buffer, thereby allowing Qualcomm HetCompute to
     *                          internally determine an optimal storage and data-transfer
     *                          policy for this buffer. This information is only used
     *                          as a hint to guide internal storage-allocation and
     *                          data-transfer optimizations, and is allowed to be
     *                          partial or incorrect.
     *
     *  @return A buffer pointer to the created buffer.
     *
     *  The optional parameter allows for the following variant to this call.
     *  @code
     *  create_buffer(T* preallocated_ptr,
     *                size_t num_elems);
     *
     *  create_buffer(T* preallocated_ptr,
     *                size_t num_elems,
                    device_set const& likely_devices);
     *  @endcode
     */
    template <typename T>
    buffer_ptr<T> create_buffer(T* preallocated_ptr, size_t num_elems, device_set const& likely_devices);

    /** @} */ /* end_addtogroup buffer_doc */

    template <typename T>
    buffer_ptr<T> create_buffer(memregion const& mr)
    {
        return create_buffer<T>(mr, 0, device_set());
    }

    template <typename T>
    buffer_ptr<T> create_buffer(memregion const& mr, size_t num_elems)
    {
        return create_buffer<T>(mr, num_elems, device_set());
    }

    template <typename T>
    buffer_ptr<T> create_buffer(memregion const& mr, device_set const& likely_devices)
    {
        return create_buffer<T>(mr, 0, likely_devices);
    }

    /** @addtogroup buffer_doc
        @{ */

    /**
     *  @brief Creates a buffer of datatype <code>T</code> of the requested size from
     *  a hetcompute::memregion.
     *
     *  Creates a buffer of datatype <code>T</code> of the requested size
     *  from a hetcompute::memregion.  The hetcompute::memregion provides initial
     *  storage and potentially initial data for the buffer.
     *  Fatal error if both <code>num_elems == 0</code> and
     *  <code>mr.get_num_bytes() == 0</code>, or if
     *  <code>(num_elems * sizeof(T)) > mr.get_num_bytes()</code>.
     *
     *
     *  @tparam T               User data type for buffer.
     *
     *  @param  mr              A memory region of the desired type allocated by the user.
     *
     *  @param  num_elems       <em>Optional</em>, default =0. If =0, the number of
     *                          elements is determined by the capacity of <code>mr</code>,
     *                          computed as follows <code>mr.get_num_bytes() / sizeof(T)</code>.
     *                          If non-zero, specifies the number of elements of type
     *                          <code>T</code> in buffer, which must fit in the capacity
     *                          of <code>mr</code>.
     *
     *  @param  likely_devices  <em>Optional</em>, default is an empty
     *                          <code>hetcompute::device_set</code>. Allows the programmer
     *                          to convey advance knowledge of which device-types
     *                          may access this buffer, thereby allowing Qualcomm HetCompute to
     *                          internally determine an optimal storage and
     *                          data-transfer policy for this buffer. This information
     *                          is only used as a hint to guide internal storage-allocation
     *                          and data-transfer optimizations, and is allowed to be
     *                          partial or incorrect.
     *
     *  @return A buffer pointer to the created buffer.
     *
     *  @sa hetcompute::memregion
     *
     *  The optional parameters allow for the following variants to this call.
     *  @code
     *  hetcompute::create_buffer(hetcompute::memregion const& mr);
     *
     *  hetcompute::create_buffer(hetcompute::memregion const& mr,
     *                          size_t num_elems);
     *
     *  hetcompute::create_buffer(hetcompute::memregion const& mr,
     *                          hetcompute::device_set const& likely_devices);
     *
     *  hetcompute::create_buffer(hetcompute::memregion const& mr,
     *                          size_t num_elems,
     *                          hetcompute::device_set const& likely_devices);
     *  @endcode
     */
    template <typename T>
    buffer_ptr<T> create_buffer(memregion const& mr, size_t num_elems, device_set const& likely_devices);

    /** @} */ /* end_addtogroup buffer_doc */

    /** @addtogroup buffer_doc
        @{ */

    /**
     *  @brief Pointer to an underlying runtime-managed buffer data structure.
     *
     *  Pointer to an underlying runtime-managed buffer data structure, if != nullptr.
     *
     *  Similar to std::shared_ptr, the buffer pointer can be assigned to another
     *  buffer pointer, compared for equality/inequality against another buffer pointer
     *  or against nullptr, and provides ref-counted access. However, HetCompute does not
     *  expose the underlying buffer as an API object. Therefore, there is no support
     *  for getting the "address-of" the buffer to assign to a buffer pointer.
     *
     *  The underlying buffer is ref-counted: it is automatically deallocated when
     *  there are no longer any buffer pointers to it. Once the programmer creates a buffer
     *  via a <code>hetcompute::create_buffer()</code> call, the user can control the
     *  lifetime of the buffer by controlling the lifetime of a buffer pointer object
     *  pointing to it.
     *
     *  @sa Shared pointers http://en.cppreference.com/w/cpp/memory/shared_ptr
     *
     */
    template <typename T>
    class buffer_ptr : private internal::buffer_ptr_base
    {
    public:
        /**
         *  Data type of buffer
         */
        using data_type = T;

        /**
         *  @brief Random access iterator providing mutable access to the buffer data.
         *
         *  Random access iterator providing mutable access to the buffer data.
         */
        typedef buffer_iterator<T> iterator;

        /**
         *  @brief Random access iterator providing immutable access to the buffer data.
         *
         *  Random access iterator providing immutable access to the buffer data.
         */
        typedef buffer_const_iterator<T> const_iterator;

        /**
         *  @brief Create a buffer_ptr with no underlying buffer storage created.
         *
         *  Create a buffer_ptr with no underlying buffer storage created. Tests equal
         *  to nullptr.
         *
         *  Example:
         *  @code
         *  hetcompute::buffer_ptr<int> b;
         *  assert(b == nullptr);
         *  @endcode
         */
        buffer_ptr() : base(), _num_elems(0)
        {
        }

        /**
         *  @brief Copy constructor: creates a new buffer_ptr pointing to the same
         *  underlying buffer as <code>other</code>.
         *
         *  Copy constructor: creates a new buffer_ptr pointing to the same underlying
         *  buffer as <code>other</code>.
         *  A <code>buffer_ptr<const T></code> instance may be constructed from an
         *  instance of <code>buffer_ptr<T></code>.
         *
         *  @param other An existing buffer pointer.
         *
         *  Example:
         *  @code
         *  hetcompute::buffer_ptr<int> b = hetcompute::create_buffer<int>(10);
         *  hetcompute::buffer_ptr<int> x(b);
         *  hetcompute::buffer_ptr<const int> y(b);
         *  @endcode
         */
        buffer_ptr(buffer_ptr<typename std::remove_const<T>::type> const& other)
            : base(reinterpret_cast<buffer_ptr_base const&>(other)), _num_elems(other.size())
        {
        }

        /**
         *  @brief Copy assignment: points to the underlying buffer of <code>other</code>.
         *
         *  Copy assignment: points to the underlying buffer of <code>other</code>.
         *  A <code>buffer_ptr<const T></code> instance may be assigned to from an instance
         *  of <code>buffer_ptr<T></code>. If this buffer_ptr was the last one pointing to
         *  its underlying buffer, the underlying buffer will get deallocated once the
         *  buffer_ptr is copy-assigned and points to a different underlying buffer.
         *
         *  @param other An existing buffer pointer.
         *
         *  Example:
         *  @code
         *  hetcompute::buffer_ptr<int> b = hetcompute::create_buffer<int>(10);
         *  hetcompute::buffer_ptr<int> x;
         *  hetcompute::buffer_ptr<const int> y;
         *  x = b;
         *  y = b;
         *  @endcode
         */
        buffer_ptr& operator=(buffer_ptr<typename std::remove_const<T>::type> const& other)
        {
            if (static_cast<void const*>(this) == &other)
                return *this;

            base::operator=(reinterpret_cast<buffer_ptr_base const&>(other));
            _num_elems    = other.size();
            return *this;
        }

        // Move constructor and move assignment are left implicit.

        /**
         * @brief Acquires the underlying buffer for read-only access by the host code.
         *
         * Acquires the underlying buffer for read-only access by the host code.
         * The host code may read the existing contents of the buffer after this call,
         * until the host code releases access using the <code>release()</code> method.
         *
         * The call will block for any conflicting operations to complete (e.g., a task
         * concurrently performing read-write access to the buffer), after which the
         * buffer is acquired for access by the host code and the call unblocks.
         * However, if the buffer has already been acquired for the host code by a preceding
         * <code>acquire_*()</code>, the call will return immediately.
         *
         * The host code may recursively acquire the buffer using a combination of
         * <code>acquire_ro()</code>, <code>acquire_wi()</code> and <code>acquire_rw()</code>
         * calls. The first <code>acquire_*</code> establishes the access type
         * (read-only, write-invalidate, or read-write) of the buffer for the host code.
         * Subsequent recursive <code>acquire_*</code> calls will succeed only if they are
         * compatible with the previously established access type. Subsequent recursive
         * <code>acquire_wi()</code> and <code>acquire_rw()</code> calls will return with
         * failure if the first recursive acquisition was <code>acquire_ro()</code>, as the
         * access type of these calls is incompatible with the established read-only access.
         * However, any subsequent <code>acquire_*()</code> recursive calls will succeed
         * if the first acquisition was either write-invalidate or read-write.
         * When the established access type is write-invalidate, subsequent recursive
         * read-only or read-write acquisitions are considered to get access to any data
         * written to the buffer after the original write-invalidate.
         * When the established access type is read-write, a subsequent recursive
         * write-invalidate does not destroy any prior data, as there is no additional
         * synchronization required between device memories to access the latest data.
         *
         * The host code releases the buffer only when a number of <code>release()</code>
         * calls equal to the number of successful recursive <code>acquire_*()</code>
         * calls are made.
         *
         * Note that access by concurrent threads of the host code is also considered
         * recursive, even when the acquire-release calls do not properly nest across
         * threads. The first acquire by any one thread establishes the host access type
         * for all threads of the host code, until the host code releases.
         *
         * HetCompute disallows concurrent access to a buffer when the buffer is being
         * modified. The acquisition will be blocked when a concurrent task/pattern
         * has acquired the buffer for read-write or write-invalidate access.
         * In rare situations, the acquisition may also be blocked when a
         * concurrent task/pattern has read-only access but HetCompute is unable
         * to synchronize the buffer data for host access until the
         * concurrent task/pattern completes.
         *
         *  @sa hetcompute::create_buffer()
         */
        void acquire_ro() const { base::acquire_ro(); }

        /**
         * @brief Attempts to acquire the underlying buffer for write-invalidate access by the host code.
         *
         * Attempts to acquire the underlying buffer for write-invalidate access by the host code.
         * Returns <code>true</code> is successful, <code>false</code> on failure to acquire for
         * write-invalidate due to a prior read-only acquisition by the host code.
         * If successful, the prior contents of the buffer are lost after this call.
         * The host code may write the buffer data (and read back what it wrote) after this call,
         * until the host code releases access using the <code>release()</code> method.
         *
         * The call will block for any conflicting operations to complete (e.g., a task
         * concurrently performing read-write access to the buffer), after which the
         * buffer is acquired for access by the host code and the call unblocks.
         * However, if the buffer has already been acquired for the host code by a preceding
         * <code>acquire_*()</code>, the call will return immediately.
         *
         * The host code may recursively acquire the buffer using a combination of
         * <code>acquire_ro()</code>, <code>acquire_wi()</code> and <code>acquire_rw()</code>
         * calls. The first <code>acquire_*</code> establishes the access type
         * (read-only, write-invalidate, or read-write) of the buffer for the host code.
         * Subsequent recursive <code>acquire_*</code> calls will succeed only if they are
         * compatible with the previously established access type. Subsequent recursive
         * <code>acquire_wi()</code> and <code>acquire_rw()</code> calls will return with
         * failure if the first recursive acquisition was <code>acquire_ro()</code>, as the
         * access type of these calls is incompatible with the established read-only access.
         * However, any subsequent <code>acquire_*()</code> recursive calls will succeed
         * if the first acquisition was either write-invalidate or read-write.
         * When the established access type is write-invalidate, subsequent recursive
         * read-only or read-write acquisitions are considered to get access to any data
         * written to the buffer after the original write-invalidate.
         * When the established access type is read-write, a subsequent recursive
         * write-invalidate does not destroy any prior data, as there is no additional
         * synchronization required between device memories to access the latest data.
         *
         * The host code releases the buffer only when a number of <code>release()</code>
         * calls equal to the number of successful recursive <code>acquire_*()</code>
         * calls are made.
         *
         * Note that access by concurrent threads of the host code is also considered
         * recursive, even when the acquire-release calls do not properly nest across
         * threads. The first acquire by any one thread establishes the host access type
         * for all threads of the host code, until the host code releases.
         *
         * HetCompute disallows concurrent access to a buffer when the buffer is being
         * modified. The acquisition will be blocked when a concurrent task/pattern
         * has acquired the buffer for read-write or write-invalidate access.
         * In rare situations, the acquisition may also be blocked when a
         * concurrent task/pattern has read-only access but HetCompute is unable
         * to synchronize the buffer data for host access until the
         * concurrent task/pattern completes.
         *
         *  @sa hetcompute::create_buffer()
         */
        bool acquire_wi() const { return base::acquire_wi(); }

        /**
         * @brief Attempts to acquire the underlying buffer for read-write access by the host code.
         *
         * Attempts to acquire the underlying buffer for read-write access by the host code.
         * Returns <code>true</code> is successful, <code>false</code> on failure to acquire for
         * read-write due to a prior read-only acquisition by the host code.
         * If successful, the host may read the prior contents of the buffer and update the contents
         * until the host code releases access using the <code>release()</code> method.
         *
         * The call will block for any conflicting operations to complete (e.g., a task
         * concurrently performing read-write access to the buffer), after which the
         * buffer is acquired for access by the host code and the call unblocks.
         * However, if the buffer has already been acquired for the host code by a preceding
         * <code>acquire_*()</code>, the call will return immediately.
         *
         * The host code may recursively acquire the buffer using a combination of
         * <code>acquire_ro()</code>, <code>acquire_wi()</code> and <code>acquire_rw()</code>
         * calls. The first <code>acquire_*</code> establishes the access type
         * (read-only, write-invalidate, or read-write) of the buffer for the host code.
         * Subsequent recursive <code>acquire_*</code> calls will succeed only if they are
         * compatible with the previously established access type. Subsequent recursive
         * <code>acquire_wi()</code> and <code>acquire_rw()</code> calls will return with
         * failure if the first recursive acquisition was <code>acquire_ro()</code>, as the
         * access type of these calls is incompatible with the established read-only access.
         * However, any subsequent <code>acquire_*()</code> recursive calls will succeed
         * if the first acquisition was either write-invalidate or read-write.
         * When the established access type is write-invalidate, subsequent recursive
         * read-only or read-write acquisitions are considered to get access to any data
         * written to the buffer after the original write-invalidate.
         * When the established access type is read-write, a subsequent recursive
         * write-invalidate does not destroy any prior data, as there is no additional
         * synchronization required between device memories to access the latest data.
         *
         * The host code releases the buffer only when a number of <code>release()</code>
         * calls equal to the number of successful recursive <code>acquire_*()</code>
         * calls are made.
         *
         * Note that access by concurrent threads of the host code is also considered
         * recursive, even when the acquire-release calls do not properly nest across
         * threads. The first acquire by any one thread establishes the host access type
         * for all threads of the host code, until the host code releases.
         *
         * HetCompute disallows concurrent access to a buffer when the buffer is being
         * modified. The acquisition will be blocked when a concurrent task/pattern
         * has acquired the buffer for read-write or write-invalidate access.
         * In rare situations, the acquisition may also be blocked when a
         * concurrent task/pattern has read-only access but HetCompute is unable
         * to synchronize the buffer data for host access until the
         * concurrent task/pattern completes.
         *
         *  @sa hetcompute::create_buffer()
         */
        bool acquire_rw() const { return base::acquire_rw(); }

        /**
         *  @brief Decrements the host acquire count, releasing the buffer from host access
         *  when the count goes to zero.
         *
         *  Decrements the host acquire count, releasing the buffer from host access
         *  when the count goes to zero.
         *  <code>release()</code> needs to be called once for every successful recursive
         *  call to <code>acquire_*()</code>, after which the buffer is released
         *  from access by the host code. The host code may not read or write
         *  the buffer contents after the final <code>release()</code> call,
         *  until the host code acquires the buffer again.
         *
         *  The <code>release()</code> call never blocks.
         *
         *  The call returns the number of recursive acquisitions remaining to be
         *  released before the host code will release access to the buffer.
         *  That is, the host code releases access when the return value of this call is 0.
         *
         *  @throw hetcompute::api_exception if called when the buffer is not currently acquired
         *  by the host code.
         *  @note  If exceptions are disabled by application, terminates the application when the 
         *         buffer is not currently acquired by the host code.
         */
        size_t release() const { return base::release(); }

        /**
         *  @brief Gets a pointer to the host accessible data of the underlying buffer,
         *  allocating if necessary.
         *
         *  Gets a pointer to the host accessible data of the underlying buffer,
         *  allocating the host accessible storage if necessary. Note that this call
         *  does not ensure that the buffer data is currently host accessible.
         *  For example, data updates by a concurrent task on the underlying buffer
         *  may not be visible yet via the host accessible data pointer.
         *
         *  @return
         *  <code>nullptr</code>, if this buffer_ptr is nullptr.\n
         *  <code>!=nullptr</code>, if this buffer_ptr points to a valid buffer.
         *
         *  @sa saved_host_data() for fast lookup of a previously queried pointer
         *  to the host accessible data.
         *
         *  @sa
         *  @code
         *  acquire_ro()
         *  acquire_wi()
         *  acquire_rw()
         *  release()
         *  @endcode
         *  to allow the buffer to be read or written by the host code, in addition
         *  to querying a pointer to the host accessible data.
         *
         *  Unlike the acquire calls, which may sometimes block when there is
         *  concurrent task access to the buffer, this method can be called at any time
         *  without blocking to determine the pointer to the host accessible data.
         */
        inline void* host_data() const
        {
            return base::host_data();
        }

        /**
         *  @brief Fast lookup of a saved pointer to the host accessible data of
         *  the underlying buffer.
         *
         *  Fast lookup of a saved pointer to the host accessible data of the
         *  underlying buffer. The pointer may be saved by either a previous 
         *  <code>host_code()</code> or <code>acquire_*</code> calls.
         *
         *  Note that this call does not ensure that the buffer data is
         *  currently host accessible via this buffer_ptr. For example, data updates by a
         *  concurrent task on the underlying buffer may not be visible yet via
         *  the host accessible data pointer.
         *
         *
         *  @return
         *  <code>nullptr</code>, if\n
         *  i) the host accessible data pointer has not previously been queried via
         *  this buffer_ptr
         *  ii) this buffer_ptr is a nullptr.\n
         *  <code>!=nullptr</code>, if\n
         *
         *  @sa host_code()
         *
         *  @sa
         *  @code
         *  acquire_ro()
         *  acquire_wi()
         *  acquire_rw()
         *  release()
         *  @endcode
         *  for explicit host synchronization.
         */
        inline void* saved_host_data() const
        {
            return base::saved_host_data();
        }

        /**
         *  @brief Allows this buffer_ptr to be passed to a gpu_kernel where a
         *  <code>hetcompute::graphics::texture_ptr</code> parameter was expected.
         *
         *  Allows this buffer_ptr to be interpreted as a texture of a given format,
         *  dimensionality and size when passed as an argument to a gpu_kernel
         *  expecting a <code>hetcompute::graphics::texture_ptr</code>.
         *  The interpretation applies to the current <code>buffer_ptr</code>, not to
         *  the buffer as a whole. That is, multiple <code>buffer_ptr</code>s to the
         *  same buffer may simultaneously be interpreted as textures of different
         *  formats, dimensions and sizes.
         *
         *  @tparam img_format The texture image format to interpret this buffer_ptr as.
         *  @tparam dims       The texture dimensions to interpret this buffer_ptr as.
         *  @param  is         The texture image size to interpret this buffer_ptr as.
         *
         *  @return This buffer_ptr.
         *
         *  Throws an assertion if the HetCompute library is built without GPU support.
        */
#ifdef HETCOMPUTE_HAVE_OPENCL
        template <hetcompute::graphics::image_format img_format, int dims>
        buffer_ptr& treat_as_texture(hetcompute::graphics::image_size<dims> const& is)
        {
            base::treat_as_texture<img_format, dims>(is);
            return *this;
        }
#endif // HETCOMPUTE_HAVE_OPENCL

        /**
         *  @brief The number of elements of datatype <code>T</code> in the underlying
         *  buffer pointed to by this buffer_ptr.
         *
         *  The number of elements of datatype <code>T</code> in the underlying buffer
         *  pointed to by this buffer_ptr.
         */
        inline size_t size() const
        {
            return _num_elems;
        }

        /**
         *  @brief Indexed lookup of buffer data.
         *
         *  If the buffer data is host accessible or being accessed as a CPU task
         *  parameter, it performs an array index lookup. Undefined behavior for
         *  host accesses if the programmer has not previously ensured that the
         *  buffer data is host accessible.
         *
         *  See <code>saved_host_data()</code> for host access criteria.
         *
         *  @param index The index to the element to lookup inside the buffer data.
         *
         *  @return A reference to the indexed element.
         *
         *  @sa at()
         *  @sa saved_host_data()
        */
        inline T& operator[](size_t index) const
        {
            return reinterpret_cast<T*>(saved_host_data())[index];
        }

        /**
         *  @brief Indexed lookup of buffer data with array-bounds and host-access
         *  allocation checks.
         *
         *  Same as operator[] but also checks if the buffer data has been previously
         *  made host accessible via this buffer_ptr and performs array bounds check.
         *  However, does not guarantee that the buffer data is currently host
         *  accessible. The programmer must ensure that the buffer will not be
         *  concurrently accessed by tasks that may invalidate the host accessible
         *  data (for example, by not launching any task that accesses a buffer_ptr
         *  to this buffer until the host access is complete).
         *
         *  See <code>saved_host_data()</code> for host access criteria.
         *
         *  @param index The index to the element to lookup inside the buffer data.
         *
         *  @throw hetcompute::api_exception if data is not host-accessible.
         *  @throw std::out_of_range if <code>index</code> exceeds array bounds.
         *  @note  If exceptions are disabled by application, the API will terminate the application
         *         if data is not host-accessible.
         *
         *  @return A reference to the indexed element.
         *
         *  @sa saved_host_data()
        */
        inline T& at(size_t index) const
        {
            // No state has been changed, so throw exceptions if
            // the buffer data is currently host accessible or if the
            // index is out of bounds.
            HETCOMPUTE_API_THROW(saved_host_data() != nullptr, "buffer data is currently not host accessible via this buffer_ptr");

            HETCOMPUTE_API_THROW_CUSTOM(0 <= index && index < _num_elems,
                                      std::out_of_range,
                                      "Out of bounds: index=%zu num_elems=%zu",
                                      index,
                                      _num_elems);

            return operator[](index);
        }

        /**
         *  @brief Iterator to the start of the buffer data.
         *
         *  Get iterator to the start of the buffer data. Allows mutable access to the
         *  buffer data.
         *
         *  @return Iterator to the start of the buffer data.
         */
        iterator begin() const
        {
            return iterator(const_cast<buffer_ptr<T>*>(this), 0);
        }

        /**
         *  @brief Iterator to the end of the buffer data.
         *
         *  Get iterator to the end of the buffer data. Allows mutable access to the
         *  buffer data.
         *
         *  @return Tterator to the end of the buffer data.
         */
        iterator end() const
        {
            return iterator(const_cast<buffer_ptr<T>*>(this), size());
        }

        /**
         *  @brief Const iterator to the start of the buffer data.
         *
         *  Get const iterator to the start of the buffer data. Restricts to immutable access.
         *
         *  @return Const iterator to the start of the buffer data.
         */
        const_iterator cbegin() const
        {
            return const_iterator(this, 0);
        }

        /**
         *  @brief Const iterator to the end of the buffer data.
         *
         *  Get const iterator to the end of the buffer data. Restricts to immutable access.
         *
         *  @return Const iterator to the end of the buffer data.
         */
        const_iterator cend() const
        {
            return const_iterator(this, size());
        }

        /**
         *  @brief Gets a string with basic information about the buffer_ptr.
         *
         *  Gets a string with basic information about the buffer_ptr.
         *
         *  @return String with basic information about the buffer_ptr.
         */
        std::string to_string() const
        {
            std::string s = ::hetcompute::internal::strprintf("num_elems=%zu sizeof(T)=%zu ", _num_elems, sizeof(T));
            s.append(base::to_string());
            return s;
        }

    private:
        /** @cond HIDE_FROM_DOXYGEN */
        typedef internal::buffer_ptr_base base;

        typedef typename std::remove_cv<T>::type bare_T;

        size_t _num_elems;

        // Not user-constructible. Use create_buffer().
        buffer_ptr(size_t num_elems, device_set const& device_hints)
            : base(num_elems * sizeof(T), device_hints), _num_elems(num_elems)
        {
        }

        buffer_ptr(T* preallocated_ptr, size_t num_elems, device_set const& device_hints)
            : base(const_cast<bare_T*>(preallocated_ptr), num_elems * sizeof(T), device_hints), _num_elems(num_elems)
        {
        }

        buffer_ptr(memregion const& mr, size_t num_elems, device_set const& device_hints)
            : base(mr, num_elems > 0 ? num_elems * sizeof(T) : mr.get_num_bytes(), device_hints),
              _num_elems(num_elems > 0 ? num_elems : mr.get_num_bytes() / sizeof(T))
        {
            HETCOMPUTE_API_THROW(num_elems == 0 || num_elems * sizeof(T) <= mr.get_num_bytes(),
                                "num_elems=%zu exceed the capacity of the memory region size=%zu bytes",
                                num_elems,
                                mr.get_num_bytes());
        }

        friend buffer_ptr<T> create_buffer<T>(size_t num_elems, device_set const& likely_devices);

        friend buffer_ptr<T>
        create_buffer<T>(T* preallocated_ptr, size_t num_elems, device_set const& likely_devices);

        friend buffer_ptr<T>
        create_buffer<T>(memregion const& mr, size_t num_elems, device_set const& likely_devices);

        inline bool is_null() const
        {
            return base::is_null();
        }

        inline void const* get_buffer() const
        {
            return base::get_buffer();
        }

        template <typename U>
        friend bool operator==(::hetcompute::buffer_ptr<U> const& b, ::std::nullptr_t);

        template <typename U>
        friend bool operator==(::std::nullptr_t, ::hetcompute::buffer_ptr<U> const& b);

        template <typename U>
        friend bool operator==(::hetcompute::buffer_ptr<U> const& b1, ::hetcompute::buffer_ptr<U> const& b2);
        /** @endcond */ /* HIDE_FROM_DOXYGEN */
    };

    /** @} */ /* end_addtogroup buffer_doc */

    /** @addtogroup buffer_doc
        @{ */

    /**
     *  @brief Scope guard for read-only acquire of a buffer by the host code.
     *
     *  Scope guard for read-only acquire of a buffer by the host code.
     *
     *  Example: instead of writing
     *  @code
     *  void f() {
     *    b.acquire_ro();
     *    if(...) {
     *      b.release();
     *      return;
     *    }
     *    ...
     *    b.release();
     *  }
     *  @endcode
     *
     *  write
     *  @code
     *  void f() {
     *    scope_acquire_ro<decltype(b)::data_type> guard(b);
     *    if(...) {
     *      return;
     *    }
     *    ...
     *  }
     *  @endcode
     *
     *  @sa buffer_ptr::acquire_ro();
     */
    template <typename T>
    class scope_acquire_ro
    {
        buffer_ptr<T> const& _b;

    public:
        explicit scope_acquire_ro(hetcompute::buffer_ptr<T> const& b) : _b(b)
        {
            HETCOMPUTE_API_ASSERT(_b != nullptr, "Cannot acquire on null buffer_ptr");
            _b.acquire_ro();
        }

        ~scope_acquire_ro() { _b.release(); }

        // Cannot default construct
        HETCOMPUTE_DELETE_METHOD(scope_acquire_ro());

        // Cannot copy
        HETCOMPUTE_DELETE_METHOD(scope_acquire_ro(scope_acquire_ro const&));
        HETCOMPUTE_DELETE_METHOD(scope_acquire_ro& operator=(scope_acquire_ro const&));

        // Cannot move
        HETCOMPUTE_DELETE_METHOD(scope_acquire_ro(scope_acquire_ro&&));
        HETCOMPUTE_DELETE_METHOD(scope_acquire_ro& operator=(scope_acquire_ro&&));
    };

    /**
     *  @brief Scope guard for write-invalidate acquire of a buffer by the host code.
     *
     *  Scope guard for write-invalidate acquire of a buffer by the host code.
     *
     *  Example: instead of writing
     *  @code
     *  void f() {
     *    b.acquire_wi();
     *    if(...) {
     *      b.release();
     *      return;
     *    }
     *    ...
     *    b.release();
     *  }
     *  @endcode
     *
     *  write
     *  @code
     *  void f() {
     *    scope_acquire_wi<decltype(b)::data_type> guard(b);
     *    if(...) {
     *      return;
     *    }
     *    ...
     *  }
     *  @endcode
     *
     *  @sa buffer_ptr::acquire_wi();
     */
    template <typename T>
    class scope_acquire_wi
    {
        buffer_ptr<T> const& _b;

    public:
        explicit scope_acquire_wi(hetcompute::buffer_ptr<T> const& b) : _b(b)
        {
            HETCOMPUTE_API_ASSERT(_b != nullptr, "Cannot acquire on null buffer_ptr");
            _b.acquire_wi();
        }

        ~scope_acquire_wi() { _b.release(); }

        // Cannot default construct
        HETCOMPUTE_DELETE_METHOD(scope_acquire_wi());

        // Cannot copy
        HETCOMPUTE_DELETE_METHOD(scope_acquire_wi(scope_acquire_wi const&));
        HETCOMPUTE_DELETE_METHOD(scope_acquire_wi& operator=(scope_acquire_wi const&));

        // Cannot move
        HETCOMPUTE_DELETE_METHOD(scope_acquire_wi(scope_acquire_wi&&));
        HETCOMPUTE_DELETE_METHOD(scope_acquire_wi& operator=(scope_acquire_wi&&));
    };

    /**
     *  @brief Scope guard for read-write acquire of a buffer by the host code.
     *
     *  Scope guard for read-write acquire of a buffer by the host code.
     *
     *  Example: instead of writing
     *  @code
     *  void f() {
     *    b.acquire_rw();
     *    if(...) {
     *      b.release();
     *      return;
     *    }
     *    ...
     *    b.release();
     *  }
     *  @endcode
     *
     *  write
     *  @code
     *  void f() {
     *    scope_acquire_rw<decltype(b)::data_type> guard(b);
     *    if(...) {
     *      return;
     *    }
     *    ...
     *  }
     *  @endcode
     *
     *  @sa buffer_ptr::acquire_rw();
     */
    template <typename T>
    class scope_acquire_rw
    {
        buffer_ptr<T> const& _b;

    public:
        explicit scope_acquire_rw(hetcompute::buffer_ptr<T> const& b) : _b(b)
        {
            HETCOMPUTE_API_ASSERT(_b != nullptr, "Cannot acquire on null buffer_ptr");
            _b.acquire_rw();
        }

        ~scope_acquire_rw() { _b.release(); }

        // Cannot default construct
        HETCOMPUTE_DELETE_METHOD(scope_acquire_rw());

        // Cannot copy
        HETCOMPUTE_DELETE_METHOD(scope_acquire_rw(scope_acquire_rw const&));
        HETCOMPUTE_DELETE_METHOD(scope_acquire_rw& operator=(scope_acquire_rw const&));

        // Cannot move
        HETCOMPUTE_DELETE_METHOD(scope_acquire_rw(scope_acquire_rw&&));
        HETCOMPUTE_DELETE_METHOD(scope_acquire_rw& operator=(scope_acquire_rw&&));
    };

    template <typename T>
    bool operator==(::hetcompute::buffer_ptr<T> const& b, ::std::nullptr_t)
    {
        return b.is_null();
    }

    template <typename T>
    bool operator==(::std::nullptr_t, ::hetcompute::buffer_ptr<T> const& b)
    {
        return b.is_null();
    }

    template <typename T>
    bool operator==(::hetcompute::buffer_ptr<T> const& b1, ::hetcompute::buffer_ptr<T> const& b2)
    {
        return b1.get_buffer() == b2.get_buffer();
    }

    template <typename T>
    bool operator!=(::hetcompute::buffer_ptr<T> const& b, ::std::nullptr_t)
    {
        return !(b == nullptr);
    }

    template <typename T>
    bool operator!=(::std::nullptr_t, ::hetcompute::buffer_ptr<T> const& b)
    {
        return !(nullptr == b);
    }

    template <typename T>
    bool operator!=(::hetcompute::buffer_ptr<T> const& b1, ::hetcompute::buffer_ptr<T> const& b2)
    {
        return !(b1 == b2);
    }

    /** @} */ /* end_addtogroup buffer_doc */

    /** @addtogroup buffer_doc
        @{ */

    /**
     *  @brief Indicates that a buffer parameter is input-only (read-only) for a kernel.
     *
     *  Use in a kernel parameter declaration to indicate that a
     *  buffer parameter will be input-only (read-only) for the kernel.
     */
    template <typename BufferPtr>
    struct in;

    /**
     *  @brief Indicates that a buffer parameter is output-only (write-invalidate) for a kernel.
     *
     *  Use in a kernel parameter declaration to indicate that a
     *  buffer parameter will be output-only (write-invalidate) for the kernel.
     */
    template <typename BufferPtr>
    struct out;

    /**
     *  @brief Indicates that a buffer parameter is used both as an input and an
     *  output (read-write) by a kernel.
     *
     *  Use in a kernel parameter declaration to indicate that a buffer parameter
     *  will be used both as an input and an output (read-write) by the kernel.
     */
    template <typename BufferPtr>
    struct inout;

    /** @} */ /* end_addtogroup buffer_doc */

}; // namespace hetcompute
