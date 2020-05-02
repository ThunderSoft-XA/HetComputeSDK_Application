#pragma once

#ifdef HETCOMPUTE_HAVE_GLES

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <hetcompute/internal/util/macros.hh>

namespace hetcompute
{
    namespace internal
    {
        // forwarding declaration
        namespace testing
        {
            class buffer_tests;
        }; // namespace testing

        // singleton class
        class egl
        {
        private:
            EGLDisplay _display;
            EGLConfig  _config;
            EGLContext _context;
            EGLSurface _surface;
            EGLint     _channel_r_bits;
            EGLint     _channel_g_bits;
            EGLint     _channel_b_bits;
            EGLint     _channel_a_bits;
            EGLint     _depth_buffer_bits;
            EGLint     _stencil_buffer_bits;
            egl();
            ~egl();

        public:
            static egl& get_instance()
            {
                static egl egl_instance;
                return egl_instance;
            }

            EGLContext get_context()
            {
                return _context;
            }

            EGLDisplay get_display()
            {
                return _display;
            }

            friend class internal::testing::buffer_tests;

        private:
            HETCOMPUTE_DELETE_METHOD(egl(egl&));
            HETCOMPUTE_DELETE_METHOD(egl(egl&&));
            HETCOMPUTE_DELETE_METHOD(egl& operator=(egl&));
            HETCOMPUTE_DELETE_METHOD(egl& operator=(egl&&));
        };

    }; // namespace internal
};  // namespace hetcompute

#endif // HETCOMPUTE_HAVE_GLES
