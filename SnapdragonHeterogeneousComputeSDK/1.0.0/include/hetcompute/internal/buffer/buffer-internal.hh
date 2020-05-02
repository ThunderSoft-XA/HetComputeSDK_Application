#pragma once

#include <string>
#include <hetcompute/devicetypes.hh>
#include <hetcompute/memregion.hh>
#include <hetcompute/texturetype.hh>

#include <hetcompute/internal/buffer/bufferstate.hh>
#include <hetcompute/internal/buffer/memregionaccessor.hh>
#include <hetcompute/internal/compat/compiler_compat.h>
#include <hetcompute/internal/graphics/texture.hh>
#include <hetcompute/internal/util/macros.hh>
#include <hetcompute/internal/util/hetcomputeptrs.hh>

// Forward declarations
namespace hetcompute
{
    template <typename T>
    class buffer_ptr;
}; // namespace hetcompute

namespace hetcompute
{
    namespace internal
    {
        // Forward declaration
        class buffer_accessor;

        struct host_access_status
        {
            /// Was the acquire/release successful
            bool _success;

            /// Multiplicity by which host code has currently acquired the buffer.
            /// Could be > 0 even if _success == false. e.g.,
            ///   when current acquire attempt is in conflict with a previous host acquire access type.
            size_t _host_acquire_multiplicity;
        };

        // Service calls involving bufferstate:
        //   calling context must lock p_bufstate if needed, the calls will access p_bufstate without lock

        void* get_or_create_host_accessible_data_ptr(bufferstate* p_bufstate, bool make_host_data_valid);

        void invalidate_non_host_arenas(bufferstate* p_bufstate);

        void setup_preallocated_buffer_data(bufferstate* p_bufstate, void* preallocated_ptr, size_t size_in_bytes);

        void setup_memregion_buffer_data(bufferstate* p_bufstate, internal_memregion* int_mr, size_t size_in_bytes);

        host_access_status host_access_buffer(bool acquire_r, bool acquire_w, bool acquire_rw, bool release, bufferstate* p_bufstate, std::unique_lock<std::mutex>& lock);

        using bufferstate_ptr = hetcompute::internal::hetcompute_shared_ptr<bufferstate>;

        struct buffer_as_texture_info
        {
#ifdef HETCOMPUTE_HAVE_OPENCL
        private:
            bool _used_as_texture;
            /// _dims, _img_format and _image_size are defined iff _used_as_texture == true
            int                                 _dims;
            hetcompute::graphics::image_format  _img_format;
            hetcompute::graphics::image_size<3> _img_size;

            hetcompute::graphics::internal::base_texture_cl* _tex_cl;

        public:
            /* implicit */
            buffer_as_texture_info(bool                                             used_as_texture = false,
                                   int                                              dims            = 0,
                                   hetcompute::graphics::image_format               img_format = hetcompute::graphics::image_format::first,
                                   hetcompute::graphics::image_size<3>              img_size   = { 0, 0, 0 },
                                   hetcompute::graphics::internal::base_texture_cl* tex_cl     = nullptr)
                : _used_as_texture(used_as_texture), _dims(dims), _img_format(img_format), _img_size(img_size), _tex_cl(tex_cl)
            {
            }

            bool                              get_used_as_texture() const { return _used_as_texture; }
            int                               get_dims() const { return _dims; }
            hetcompute::graphics::image_format  get_img_format() const { return _img_format; }
            hetcompute::graphics::image_size<3> get_img_size() const { return _img_size; }

            hetcompute::graphics::internal::base_texture_cl*& access_tex_cl() { return _tex_cl; }

#else       // HETCOMPUTE_HAVE_OPENCL
            buffer_as_texture_info() {}

            bool get_used_as_texture() const { return false; }
#endif      // HETCOMPUTE_HAVE_OPENCL
        };  // struct buffer_as_texture_info

        class buffer_ptr_base
        {
        private:
            /// The underlying buffer pointed to by this buffer_ptr_base.
            /// Possibly null.
            bufferstate_ptr _bufstate;

            /// Pointer to data accessible by the host.
            /// Semantics:
            ///  == nullptr => host has not previously acquired the underlying buffer
            ///                via this buffer_ptr.
            ///
            ///  != nullptr => host has previously acquired the underlying buffer
            ///                via this buffer_ptr and _host_accessible_data points
            ///                to the buffer data. However, it is invalid to access
            ///                the buffer data unless the host has currently acquired
            ///                the underlying buffer via this or another buffer_ptr to
            ///                the underlying buffer.
            ///
            /// Note: acquire_ro(), acquire_wi(), acquire_rw() methods acquire the
            //        underlying buffer, while the release() method releases it.
            mutable void* _host_accessible_data;

            buffer_as_texture_info _tex_info;

            friend class buffer_accessor;

        protected:
            // Create buffer_ptr with no bufferstate
            buffer_ptr_base() : _bufstate(nullptr), _host_accessible_data(nullptr), _tex_info()
            {
            }

            // Construction of fresh buffer
            buffer_ptr_base(size_t size_in_bytes, device_set const& device_hints)
                : _bufstate(new bufferstate(size_in_bytes, device_hints)),
                  _host_accessible_data(nullptr),
                  _tex_info()
            {
            }

            buffer_ptr_base(void* preallocated_ptr, size_t size_in_bytes, device_set const& device_hints)
                : _bufstate(new bufferstate(size_in_bytes, device_hints)),
                  _host_accessible_data(nullptr),
                  _tex_info()
            {
                HETCOMPUTE_API_THROW(preallocated_ptr != nullptr, "null preallocated region");
                setup_preallocated_buffer_data(::hetcompute::internal::c_ptr(_bufstate), preallocated_ptr, size_in_bytes);
            }

            buffer_ptr_base(memregion const& mr, size_t size_in_bytes, device_set const& device_hints)
                : _bufstate(new bufferstate(size_in_bytes, device_hints)),
                  _host_accessible_data(nullptr),
                  _tex_info()
            {
                auto int_mr = ::hetcompute::internal::memregion_base_accessor::get_internal_mr(mr);
                HETCOMPUTE_INTERNAL_ASSERT(int_mr != nullptr, "Error.Internal memregion object is null");
                HETCOMPUTE_INTERNAL_ASSERT(int_mr->get_type() != ::hetcompute::internal::memregion_t::none, "Error. Invalid type of memregion");
                setup_memregion_buffer_data(::hetcompute::internal::c_ptr(_bufstate), int_mr, size_in_bytes);
            }

            HETCOMPUTE_DEFAULT_METHOD(buffer_ptr_base(buffer_ptr_base const&));
            HETCOMPUTE_DEFAULT_METHOD(buffer_ptr_base& operator=(buffer_ptr_base const&));
            HETCOMPUTE_DEFAULT_METHOD(buffer_ptr_base(buffer_ptr_base&&));
            HETCOMPUTE_DEFAULT_METHOD(buffer_ptr_base& operator=(buffer_ptr_base&&));

        public:
            inline void* host_data() const
            {
                // Once allocated, the host data pointer will not change
                if (saved_host_data() != nullptr)
                {
                    return saved_host_data();
                }

                auto p_bufstate = ::hetcompute::internal::c_ptr(_bufstate);
                if (p_bufstate == nullptr)
                {
                    return nullptr; // no associated bufferstate
                }

                std::lock_guard<std::mutex> lock(p_bufstate->access_mutex());
                allocate_host_accessible_data(false);
                return saved_host_data();
            }

            inline void* saved_host_data() const
            {
                return _host_accessible_data;
            }

            /// Ensures that the mainmem-arena is created, storage allocated and contains valid data
            void allocate_host_accessible_data(bool make_host_data_valid) const
            {
                auto p_bufstate = ::hetcompute::internal::c_ptr(_bufstate);
                if (p_bufstate == nullptr)
                {
                    return; // no associated bufferstate
                }

                // use make_host_data_valid = true only if the host-code is allowed to access the data
                auto tmp = get_or_create_host_accessible_data_ptr(p_bufstate, make_host_data_valid);
                HETCOMPUTE_INTERNAL_ASSERT(_host_accessible_data == nullptr || _host_accessible_data == tmp,
                                         "host accessible pointer must not change: was=%p now=%p",
                                         _host_accessible_data,
                                         tmp);
                _host_accessible_data = tmp;
                HETCOMPUTE_INTERNAL_ASSERT(_host_accessible_data != nullptr, "allocate_host_accessible_data failed");
            }

            inline void acquire_ro() const
            {
                auto p_bufstate = ::hetcompute::internal::c_ptr(_bufstate);
                HETCOMPUTE_API_THROW(p_bufstate != nullptr, "acquire_ro(): ERROR: no associated bufferstate");

                std::unique_lock<std::mutex> lock(p_bufstate->access_mutex());
                auto has = host_access_buffer(true, false, false, false, p_bufstate, lock);
                HETCOMPUTE_INTERNAL_ASSERT(has._success, "acquire_ro() should always succeed");
                if (_host_accessible_data == nullptr)
                {
                    allocate_host_accessible_data(true);
                }
                HETCOMPUTE_INTERNAL_ASSERT(_host_accessible_data != nullptr,
                                         "Unexpected nullptr for host data after grant of blocking host access request");
            }

            inline bool acquire_wi() const
            {
                auto p_bufstate = ::hetcompute::internal::c_ptr(_bufstate);
                HETCOMPUTE_API_THROW(p_bufstate != nullptr, "acquire_wi(): ERROR: no associated bufferstate");

                std::unique_lock<std::mutex> lock(p_bufstate->access_mutex());
                auto has = host_access_buffer(false, true, false, false, p_bufstate, lock);
                if (!has._success)
                {
                    return false;
                }

                if (_host_accessible_data == nullptr)
                {
                    allocate_host_accessible_data(true);
                }
                HETCOMPUTE_INTERNAL_ASSERT(_host_accessible_data != nullptr,
                                         "Unexpected nullptr for host data after grant of blocking host access request");
                return true;
            }

            inline bool acquire_rw() const
            {
                auto p_bufstate = ::hetcompute::internal::c_ptr(_bufstate);
                HETCOMPUTE_API_THROW(p_bufstate != nullptr, "acquire_rw(): ERROR: no associated bufferstate");

                std::unique_lock<std::mutex> lock(p_bufstate->access_mutex());
                auto has = host_access_buffer(false, false, true, false, p_bufstate, lock);
                if (!has._success)
                {
                    return false;
                }

                if (_host_accessible_data == nullptr)
                {
                    allocate_host_accessible_data(true);
                }
                HETCOMPUTE_INTERNAL_ASSERT(_host_accessible_data != nullptr,
                                         "Unexpected nullptr for host data after grant of blocking host access request");
                return true;   
            }

            inline size_t release() const
            {
                auto p_bufstate = ::hetcompute::internal::c_ptr(_bufstate);
                HETCOMPUTE_API_THROW(p_bufstate != nullptr, "release(): ERROR: no associated bufferstate");

                std::unique_lock<std::mutex> lock(p_bufstate->access_mutex());
                auto has = host_access_buffer(false, false, false, true, p_bufstate, lock);
                return has._host_acquire_multiplicity;
            }

            std::string to_string() const
            {
                std::string s =
                    ::hetcompute::internal::strprintf("host_ptr=%p bufstate=%p", _host_accessible_data, ::hetcompute::internal::c_ptr(_bufstate));
                return s;
            }

#ifdef HETCOMPUTE_HAVE_OPENCL
            template <hetcompute::graphics::image_format img_format, int dims>
            void treat_as_texture(hetcompute::graphics::image_size<dims> const& is)
            {
                _tex_info =
                    buffer_as_texture_info(true, dims, img_format, hetcompute::graphics::internal::image_size_converter<3, dims>::convert(is));
            }
#endif // HETCOMPUTE_HAVE_OPENCL

            inline bool is_null() const
            {
                return ::hetcompute::internal::c_ptr(_bufstate) == nullptr;
            }

            inline void const* get_buffer() const
            {
                return ::hetcompute::internal::c_ptr(_bufstate);
            }
        };  // class buffer_ptr_base

        class buffer_accessor
        {
        public:
            static bufferstate_ptr get_bufstate(buffer_ptr_base& bb)
            {
                return bb._bufstate;
            }

            static bufferstate_ptr const get_bufstate(buffer_ptr_base const& bb)
            {
                return bb._bufstate;
            }

            static buffer_as_texture_info get_buffer_as_texture_info(buffer_ptr_base const& bb)
            {
                return bb._tex_info;
            }

            static void set_name_locked(buffer_ptr_base const& bb, std::string const& name)
            {
                auto p_bufstate = hetcompute::internal::c_ptr(bb._bufstate);
                HETCOMPUTE_API_THROW(p_bufstate != nullptr, "buffer_ptr does not point to a valid buffer");
                std::lock_guard<std::mutex> lock(p_bufstate->access_mutex());
                p_bufstate->set_name(name);
            }

            template <typename T>
            static void set_name_locked(buffer_ptr<T> const& b, std::string const& name)
            {
                set_name_locked(reinterpret_cast<buffer_ptr_base const&>(b), name);
            }

            static std::string get_state_string_locked(buffer_ptr_base const& bb)
            {
                auto p_bufstate = hetcompute::internal::c_ptr(bb._bufstate);
                HETCOMPUTE_API_THROW(p_bufstate != nullptr, "buffer_ptr does not point to a valid buffer");
                std::lock_guard<std::mutex> lock(p_bufstate->access_mutex());
                return p_bufstate->to_string();
            }

            template <typename T>
            static std::string get_state_string_locked(buffer_ptr<T> const& b)
            {
                return get_state_string_locked(reinterpret_cast<buffer_ptr_base const&>(b));
            }

            static void collect_statistics_locked(buffer_ptr_base const& bb, bool enable, bool print_at_buffer_dealloc = true)
            {
                auto p_bufstate = hetcompute::internal::c_ptr(bb._bufstate);
                HETCOMPUTE_API_THROW(p_bufstate != nullptr, "buffer_ptr does not point to a valid buffer");
                std::lock_guard<std::mutex> lock(p_bufstate->access_mutex());
                p_bufstate->collect_statistics(enable, print_at_buffer_dealloc);
            }

            template <typename T>
            static void collect_statistics_locked(buffer_ptr<T> const& b, bool enable, bool print_at_buffer_dealloc = true)
            {
                collect_statistics_locked(reinterpret_cast<buffer_ptr_base const&>(b), enable, print_at_buffer_dealloc);
            }

            HETCOMPUTE_DELETE_METHOD(buffer_accessor());
            HETCOMPUTE_DELETE_METHOD(buffer_accessor(buffer_accessor const&));
            HETCOMPUTE_DELETE_METHOD(buffer_accessor& operator=(buffer_accessor const&));
            HETCOMPUTE_DELETE_METHOD(buffer_accessor(buffer_accessor&&));
            HETCOMPUTE_DELETE_METHOD(buffer_accessor& operator=(buffer_accessor&&));
        }; // class buffer_accessor
    }; // namespace internal
};     // namespace hetcompute

namespace hetcompute
{
    template <typename T>
    buffer_ptr<T> create_buffer(size_t num_elems, device_set const& likely_devices)
    {
        HETCOMPUTE_API_THROW(num_elems > 0, "create_buffer(): cannot create empty buffer.");
        buffer_ptr<T> b(num_elems, likely_devices);
        return b;
    }

    template <typename T>
    buffer_ptr<T> create_buffer(T* preallocated_ptr, size_t num_elems, device_set const& likely_devices)
    {
        HETCOMPUTE_API_THROW(num_elems > 0, "create_buffer(): cannot create empty buffer.");
        buffer_ptr<T> b(preallocated_ptr, num_elems, likely_devices);
        return b;
    }

    template <typename T>
    buffer_ptr<T> create_buffer(memregion const& mr, size_t num_elems, device_set const& likely_devices)
    {
        HETCOMPUTE_API_THROW(mr.get_num_bytes() > 0 || num_elems > 0, "create_buffer(): cannot create empty buffer.");
        buffer_ptr<T> b(mr, num_elems, likely_devices);
        return b;
    }
}; // namespace hetcompute

namespace hetcompute
{
    template <typename BufferPtr>
    struct in
    {
        using buffer_type = BufferPtr;
    };

    template <typename BufferPtr>
    struct out
    {
        using buffer_type = BufferPtr;
    };

    template <typename BufferPtr>
    struct inout
    {
        using buffer_type = BufferPtr;
    };
}; // namespace hetcompute
