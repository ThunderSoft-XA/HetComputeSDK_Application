#pragma once

#include <hetcompute/internal/storage/storagemap.hh>

namespace hetcompute
{
    namespace internal
    {
        // device_thread_id is used in both devicethread.hh and events.hh
        typedef unsigned short device_thread_id;
        typedef unsigned short dsp_device_thread_id;

        // C-like API, similar to the pthread TLS API, currently internal
        int   sls_key_create(storage_key*, void (*dtor)(void*));
        int   sls_set_specific(storage_key, void const*);
        void* sls_get_specific(storage_key);
    };  //namespace internal
};  // namespace hetcompute
