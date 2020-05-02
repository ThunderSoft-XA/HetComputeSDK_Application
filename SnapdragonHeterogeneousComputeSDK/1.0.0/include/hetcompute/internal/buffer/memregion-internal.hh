#pragma once

#include <cstddef>

#include <hetcompute/internal/compat/compiler_compat.h>

#ifdef HETCOMPUTE_HAVE_OPENCL
#include <hetcompute/internal/device/cldevice.hh>
#include <hetcompute/internal/device/gpuopencl.hh>
#endif // HETCOMPUTE_HAVE_OPENCL

#ifdef HETCOMPUTE_HAVE_GLES
#include <hetcompute/internal/device/gpuopengl.hh>
#endif // HETCOMPUTE_HAVE_GLES

#include <hetcompute/internal/memalloc/alignedmalloc.hh>
#include <hetcompute/internal/util/debug.hh>
#include <hetcompute/internal/util/macros.hh>
#include <hetcompute/internal/util/scopeguard.hh>

namespace hetcompute
{
    namespace internal
    {
#ifdef HETCOMPUTE_HAVE_OPENCL
        extern std::vector<legacy::device_ptr>* s_dev_ptrs;
#endif // HETCOMPUTE_HAVE_OPENCL

        // Internal memory types
        enum class memregion_t
        {
            none,
            main,
            cl2svm,
            ion,
            glbuffer
        };

        class internal_memregion
        {
        public:
            internal_memregion() : _num_bytes(0)
            {
            }

            explicit internal_memregion(size_t sz) : _num_bytes(sz)
            {
            }

            virtual ~internal_memregion()
            {
            }

            virtual memregion_t get_type() const
            {
                return memregion_t::none;
            }

            virtual void* get_ptr(void) const
            {
                HETCOMPUTE_FATAL("Calling get_ptr() method of base class internal_memregion.");
                return nullptr;
            }

#ifdef HETCOMPUTE_HAVE_GLES
            virtual GLuint get_id() const
            {
                HETCOMPUTE_FATAL("Calling get_id() method of base class internal_memregion.");
            }
#else  /// HETCOMPUTE_HAVE_GLES
            virtual int get_id() const
            {
                HETCOMPUTE_FATAL("Calling get_id() method of base class internal_memregion.");
            }
#endif /// HETCOMPUTE_HAVE_GLES

            size_t get_num_bytes() const
            {
                return _num_bytes;
            }

            virtual bool is_cacheable() const
            {
                return false;
            }

            virtual int get_fd() const
            {
                HETCOMPUTE_FATAL("Calling get_fd of base class internal_memregion.");
                return -1;
            }

        protected:
            size_t _num_bytes;

            HETCOMPUTE_DELETE_METHOD(internal_memregion(internal_memregion const&));
            HETCOMPUTE_DELETE_METHOD(internal_memregion& operator=(internal_memregion const&));
            HETCOMPUTE_DELETE_METHOD(internal_memregion(internal_memregion&&));
            HETCOMPUTE_DELETE_METHOD(internal_memregion& operator=(internal_memregion&&));
        };  // class internal_memregion

        class internal_main_memregion : public internal_memregion
        {
        private:
            void* _p;
            bool  _is_internally_allocated;

        public:
            internal_main_memregion(size_t sz, size_t alignment) : internal_memregion(sz), _p(nullptr), _is_internally_allocated(true)
            {
                HETCOMPUTE_API_THROW(sz > 0, "Error. Specified size of mem region is <=0");
                _p = ::hetcompute::internal::hetcompute_aligned_malloc(alignment, sz);
            }

            // It is assumed here that the memory pointed to by ptr has a lifetime that exceeds the use by
            // the memregion object. The user is responsible for ensuring this.
            internal_main_memregion(void* ptr, size_t sz) : internal_memregion(sz), _p(ptr), _is_internally_allocated(false)
            {
                // Assert that ptr is not nullptr. No other checks with regard to size of the externally allocated
                // chunk are performed.
                HETCOMPUTE_API_THROW(ptr != nullptr, "Error. Expect non nullptr for externally allocated memregion");
            }

            ~internal_main_memregion()
            {
                // deallocate memory only if it was internally allocated.
                if (_is_internally_allocated)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(_p != nullptr, "Error. mem region pointer is nullptr");
                    ::hetcompute::internal::hetcompute_aligned_free(_p);
                }
            }

            void* get_ptr(void) const
            {
                return _p;
            }

            memregion_t get_type() const
            {
                return memregion_t::main;
            }

            HETCOMPUTE_DELETE_METHOD(internal_main_memregion(internal_main_memregion const&));
            HETCOMPUTE_DELETE_METHOD(internal_main_memregion& operator=(internal_main_memregion const&));
            HETCOMPUTE_DELETE_METHOD(internal_main_memregion(internal_main_memregion&&));
            HETCOMPUTE_DELETE_METHOD(internal_main_memregion& operator=(internal_main_memregion&&));
        }; // class internal_main_memregion

#ifdef HETCOMPUTE_HAVE_QTI_DSP
        class internal_ion_memregion : public internal_memregion
        {
        private:
            bool  _cacheable;
            void* _p;
            bool  _is_internally_allocated;
            int   _fd;

        public:
            internal_ion_memregion(size_t sz, bool cacheable);

            // It is assumed here that the ION memory pointed to by ptr has a lifetime that exceeds the use by
            // the memregion object. The user is responsible for ensuring this.
            internal_ion_memregion(void* ptr, size_t sz, bool cacheable);

            internal_ion_memregion(void* ptr, int fd, size_t sz, bool cacheable);

            ~internal_ion_memregion();

            void* get_ptr(void) const
            {
                return _p;
            }

            memregion_t get_type() const
            {
                return memregion_t::ion;
            }

            bool is_cacheable() const
            {
                return _cacheable;
            }

            int get_fd() const
            {
                return _fd;
            }

            HETCOMPUTE_DELETE_METHOD(internal_ion_memregion(internal_ion_memregion const&));
            HETCOMPUTE_DELETE_METHOD(internal_ion_memregion& operator=(internal_ion_memregion const&));
            HETCOMPUTE_DELETE_METHOD(internal_ion_memregion(internal_ion_memregion&&));
            HETCOMPUTE_DELETE_METHOD(internal_ion_memregion& operator=(internal_ion_memregion&&));
        };
#endif // HETCOMPUTE_HAVE_QTI_DSP

#ifdef HETCOMPUTE_HAVE_GLES
        class internal_glbuffer_memregion : public internal_memregion
        {
        private:
            GLuint _id;

        public:
            explicit internal_glbuffer_memregion(GLuint id) : internal_memregion(), _id(id)
            {
                // get the original value of GL_ARRAY_BUFFER binding
                GLint old_value;
                glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &old_value);

                auto guard = make_scope_guard([old_value] {
                    glBindBuffer(GL_ARRAY_BUFFER, old_value);
                    GLenum error = glGetError();
                    HETCOMPUTE_API_THROW((error == GL_NO_ERROR), "glBindBuffer(0)->%x", error);
                });

                // Bind the buffer to GL_ARRAY_BUFFER slot
                glBindBuffer(GL_ARRAY_BUFFER, _id);
                GLenum error = glGetError();

                // We will throw an API exception because the errors are outside
                // the user's and HETCOMPUTE's control.
                HETCOMPUTE_API_THROW((error == GL_NO_ERROR), "glBindBuffer(%u)->%x", _id, error);
                HETCOMPUTE_API_THROW((glIsBuffer(_id) == GL_TRUE), "Invalid GL buffer");

                // Get the GL buffer object size.
                GLint gl_buffer_size = 0;
                glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &gl_buffer_size);
                error = glGetError();
                HETCOMPUTE_API_THROW((error == GL_NO_ERROR), "glGetBufferParameteriv()->%d", error);

                _num_bytes = gl_buffer_size;
                HETCOMPUTE_DLOG("GL buffer object size: %zu", _num_bytes);
                HETCOMPUTE_API_THROW(_num_bytes != 0, "GL buffer size can't be 0");
            }

            GLuint get_id() const
            {
                return _id;
            }

            memregion_t get_type() const
            {
                return memregion_t::glbuffer;
            }

            HETCOMPUTE_DELETE_METHOD(internal_glbuffer_memregion(internal_glbuffer_memregion const&));
            HETCOMPUTE_DELETE_METHOD(internal_glbuffer_memregion& operator=(internal_glbuffer_memregion const&));
            HETCOMPUTE_DELETE_METHOD(internal_glbuffer_memregion(internal_glbuffer_memregion&&));
            HETCOMPUTE_DELETE_METHOD(internal_glbuffer_memregion& operator=(internal_glbuffer_memregion&&));
        };
#endif  // HETCOMPUTE_HAVE_GLES

    };     // namespace internal
    };     // namespace hetcompute
