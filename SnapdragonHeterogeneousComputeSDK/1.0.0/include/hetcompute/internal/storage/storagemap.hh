#pragma once

#include <vector>
#include <memory>
#include <hetcompute/internal/util/macros.hh>
#include <iostream>

namespace hetcompute
{
    namespace internal
    {
        /// \brief storage key, similar to `pthread_key_t`
        struct storage_key
        {
            size_t id;
            void (*dtor)(void*);
        };  // struct storage_key

        /// \brief map of per-task task-local storage values
        class storage_map
        {
        private:
            /// \brief internal record for one task-local storage entry
            struct task_storage_item
            {
                task_storage_item() : data(nullptr), dtor(nullptr)
                {
                }

                task_storage_item(void const* data_, void (*dtor_)(void*)) : data(data_), dtor(dtor_)
                {
                }

                void const* data;                   /// data to store
                void (*dtor)(void*);                /// destructor to run at end of life-time, or null
            };

            std::unique_ptr<std::vector<task_storage_item>> _map;

        public:
            storage_map() : _map(nullptr)
            {
            }

            HETCOMPUTE_DELETE_METHOD(storage_map(storage_map const&));
            HETCOMPUTE_DELETE_METHOD(const storage_map& operator=(storage_map const&));

            ~storage_map()
            {
                dispose();
            }

            /// \brief returns value associated to key
            /// \param key key to look up
            void* get_specific(storage_key key)
            {
                if (_map && key.id < _map->size())
                {
                    return const_cast<void*>((*_map)[key.id].data);
                }
                return nullptr;
            }

            /// \brief associates key and value
            /// \param key key to associate value with
            /// \param value value to associate with key
            void set_specific(storage_key key, void const* value)
            {
                ensure_map_size(key.id + 1);
                (*_map)[key.id] = task_storage_item(value, key.dtor);
            }

            /// \brief disposes entire map, running any registered destructors
            ///   for each non-null value.
            void dispose()
            {
                if (!_map)
                {
                    return;
                }

                // call dtors
                for (auto& item : *_map)
                {
                    if (item.data && item.dtor)
                    {
                        (*item.dtor)(const_cast<void*>(item.data));
                    }
                }
                _map.reset();
            }

        private:
            /// \brief ensures that map exists and has at least a certain size
            /// \param size size to ensure
            void ensure_map_size(size_t size)
            {
                if (!_map)
                {
                    _map = std::unique_ptr<std::vector<task_storage_item>>(new std::vector<task_storage_item>(size));
                }
                else if (_map->size() < size)
                {
                    _map->resize(size, task_storage_item());
                }
            }
        };  // class storage_map

    };  // namespace internal
};  // namespace hetcompute
