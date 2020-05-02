#pragma once

#include <hetcompute/internal/storage/storagemap.hh>

namespace hetcompute
{
    namespace internal
    {
        // C-like API, similar to the pthread TLS API, currently internal
        int   task_key_create(storage_key*, void (*dtor)(void*));
        int   task_set_specific(storage_key, void const*);
        void* task_get_specific(storage_key);
    };  // namespace internal
};  // namespace hetcompute
