#pragma once

#ifdef HETCOMPUTE_HAVE_QTI_DSP

#include <hetcompute/internal/legacy/types.hh>
#include <hetcompute/internal/util/hetcomputeptrs.hh>

namespace hetcompute
{
    namespace internal
    {
        // Class representing DSP device. We might store some device data in this class
        class dspdevice : public ref_counted_object<dspdevice>
        {
        public:
            // Initializes rpc mem and dsp msg
            static void init();

            // Shuts down rpc mem and dsp msg
            static void shutdown();

            // Allocate ion memory
            static void* allocate(size_t size);

            // Deallocate ion memory
            static void deallocate(void* mem);
        };  // class dspdevice

        /// dspbuffer provides an abstraction to hetcompute::buffer
        /// to manage the data movements to/from dsp. Currently
        /// we use ION memory heaps.
        class dspbuffer : public ref_counted_object<dspbuffer>
        {
        protected:
            /// Pointer to the host pointer
            /// in case of ion it is the same as the ion pointer
            void* _host_ptr;

            /// The size of the buffer in bytes
            size_t _size;

            /// Will be true if the buffer allocated from DSP special ION heap (true)
            /// or provided by user (false)
            bool _device_allocated;

            /// Indicates that this buffer might also be used by GPU as well
            bool _fully_shared;

            HETCOMPUTE_DELETE_METHOD(dspbuffer(dspbuffer const&));
            HETCOMPUTE_DELETE_METHOD(dspbuffer(dspbuffer&&));
            HETCOMPUTE_DELETE_METHOD(dspbuffer& operator=(dspbuffer const&));
            HETCOMPUTE_DELETE_METHOD(dspbuffer& operator=(dspbuffer&&));

        public:
            /// Constructs ion DSP buffer
            /// @param size: the size of the buffer
            dspbuffer(size_t size)
                : _host_ptr(nullptr), _size(size), _device_allocated(true), _fully_shared(false)
            {
                _host_ptr = dspdevice::allocate(size);
                HETCOMPUTE_API_THROW(_host_ptr != nullptr, "rpcmem_alloc failed");
            }

            /// Constructs non-ion DSP buffer
            /// @param size: the size of the buffer
            /// @param ptr: the host allocated memory (non-ion)
            dspbuffer(size_t size, void* ptr)
                : _host_ptr(ptr), _size(size), _device_allocated(false), _fully_shared(false)
            {
            }

            virtual ~dspbuffer()
            {
                // Move the data back to the device
                copy_to_device();

                // Free up the memory
                if (is_device_allocated() && _host_ptr != nullptr)
                {
                    dspdevice::deallocate(_host_ptr);
                }
            }

            // Sets the flag indicating this buffer will be shared
            void set_fully_shared()
            {
                _fully_shared = true;
            }

            bool is_fully_shared()
            {
                return _fully_shared;
            }

            bool is_device_allocated()
            {
                return _device_allocated;
            }

            void* get_host_ptr()
            {
                return _host_ptr;
            }

            virtual void copy_to_device()
            {
                // Practically there is nothing to be done to copy
                // the buffer from CPU or GPU to DSP as all the cache
                // flush/invalidation is handled by CPU/DSP kernels
            }

            virtual void* copy_from_device()
            {
                return _host_ptr;
            }

            /// Releases the buffer from the device
            void release()
            {
                copy_from_device();
            }
        };  // class dspbuffer

        /// Creates an dsp buffer (non-ion)
        /// @param ptr: the host memory allocated
        /// @param size: the size of the buffer
        inline legacy::dspbuffer_ptr create_dsp_buffer(void* ptr, size_t size)
        {
            internal::dspbuffer* b_ptr = new internal::dspbuffer(size, ptr);
            HETCOMPUTE_DLOG("Creating hetcompute::internal::dspbuffer %p", b_ptr);
            return legacy::dspbuffer_ptr(b_ptr, legacy::dspbuffer_ptr::ref_policy::do_initial_ref);
        }

        /// Creates an dsp buffer (ion)
        /// @param size: the size of the buffer
        inline legacy::dspbuffer_ptr create_dsp_buffer(size_t size)
        {
            internal::dspbuffer* b_ptr = new internal::dspbuffer(size);
            HETCOMPUTE_DLOG("Creating hetcompute::internal::dspbuffer %p", b_ptr);
            return legacy::dspbuffer_ptr(b_ptr, legacy::dspbuffer_ptr::ref_policy::do_initial_ref);
        }

    };  // namespace internal
};  // namespace hetcompute

#endif // HETCOMPUTE_HAVE_QTI_DSP
