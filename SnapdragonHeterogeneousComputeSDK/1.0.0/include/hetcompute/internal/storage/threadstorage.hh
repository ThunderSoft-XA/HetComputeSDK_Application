#pragma once

#include <hetcompute/internal/storage/storagemap.hh>

namespace hetcompute
{
    namespace internal
    {
        // C-like API, similar to the pthread TLS API, currently internal
        int   tls_key_create(storage_key*, void (*dtor)(void*));
        int   tls_set_specific(storage_key, void const*);
        void* tls_get_specific(storage_key);
    };  // namespace internal
};  // namespace hetcompute
