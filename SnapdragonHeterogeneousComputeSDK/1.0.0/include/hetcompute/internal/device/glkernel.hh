#pragma once

#ifdef HETCOMPUTE_HAVE_GLES

#include <mutex>
#include <string>
#include <hetcompute/internal/device/gpuopengl.hh>
#include <hetcompute/range.hh>

namespace hetcompute
{
    namespace internal
    {
        class glkernel
        {
        private:
            static std::mutex s_dispatch_mutex; // a global mutex to serialize all GL kernel executions
            GLuint            _prog_handle;
            GLuint            _shader_handle;
            GLsync            _gl_fence;
            static const int  SHADER_LOG_SIZE = 10240;

        public:
            explicit glkernel(const std::string& task_str) : _prog_handle(0), _shader_handle(0), _gl_fence(0)
            {
                _prog_handle      = glCreateProgram();
                _shader_handle    = glCreateShader(GL_COMPUTE_SHADER);
                auto glShaderCode = task_str.c_str();
                glShaderSource(_shader_handle, 1, &glShaderCode, nullptr);
                glCompileShader(_shader_handle);

                // Get Build log
                int rvalue;
                glGetShaderiv(_shader_handle, GL_COMPILE_STATUS, &rvalue);
                GLchar  log[SHADER_LOG_SIZE];
                GLsizei length;
                if (!rvalue)
                {
                    glGetShaderInfoLog(_shader_handle, SHADER_LOG_SIZE - 1, &length, log);
                    HETCOMPUTE_FATAL("glCompileShader()->%d\n build_log: %s", rvalue, log);
                }
                glAttachShader(_prog_handle, _shader_handle);

                // Get link log
                glLinkProgram(_prog_handle);
                glGetProgramiv(_prog_handle, GL_LINK_STATUS, &rvalue);
                if (!rvalue)
                {
                    glGetProgramInfoLog(_prog_handle, SHADER_LOG_SIZE - 1, &length, log);
                    HETCOMPUTE_FATAL("glLinkProgram()->%d\n linker_log: %s", rvalue, log);
                }
            }

            inline std::mutex& access_dispatch_mutex()
            {
                return s_dispatch_mutex;
            }

            inline void set_arg(size_t index, GLuint gl_buffer)
            {
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, gl_buffer);
            }

            // handle value types.
            template <typename T>
            void set_arg(size_t, T)
            {
                HETCOMPUTE_UNIMPLEMENTED("GL path does not yet support value args");
            }

            // handle local args.
            void set_arg_local_alloc(size_t, size_t)
            {
                HETCOMPUTE_UNIMPLEMENTED("GL path does not yet support local alloc args");
            }

            template <size_t Dims>
            GLsync launch(const ::hetcompute::range<Dims>& r, const ::hetcompute::range<Dims>& lr);

            GLsync get_sync_object()
            {
                return _gl_fence;
            }

        };  // class glkernel

    };  // namespace internal
};  // namespace hetcompute

#endif // HETCOMPUTE_HAVE_GLES
