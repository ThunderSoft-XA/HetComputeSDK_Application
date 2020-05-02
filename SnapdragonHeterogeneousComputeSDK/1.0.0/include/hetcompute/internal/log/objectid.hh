#pragma once

#include <atomic>
#include <cstdint>
#include <hetcompute/internal/compat/compiler_compat.h>
#include <hetcompute/internal/util/memorder.hh>

namespace hetcompute
{
    namespace internal
    {
        namespace log
        {
            // Forward declarations so that I can put
            // the static assert within object_id_base
            template <typename ObjectType>
            class null_object_id;
            template <typename ObjectType>
            class seq_object_id;

            /// Base template class of the object ids used by
            /// the logger.
            template <typename ObjectType>
            class object_id_base
            {
            public:
                typedef std::uint32_t raw_id_type;
                typedef ObjectType    object_type;

                raw_id_type get_raw_value() const
                {
                    static_assert(sizeof(null_object_id<object_type>) == sizeof(seq_object_id<object_type>),
                                  "null_object_id and seq_object_id are of differnt sizes");
                    return _id;
                }

            protected:
                enum commom_ids : raw_id_type
                {
                    s_invalid_object_id     = 0,
                    s_first_valid_object_id = 1
                };

                object_id_base() : _id(s_invalid_object_id)
                {
                }

                explicit object_id_base(raw_id_type id) : _id(id)
                {
                    HETCOMPUTE_INTERNAL_ASSERT(id >= s_first_valid_object_id, "Invalid object id.");
                }

            private:
                const raw_id_type _id;
            };

            /// Always-zero object id
            template <typename ObjectType>
            class null_object_id : public object_id_base<ObjectType>
            {
                typedef object_id_base<ObjectType> parent;

            public:
                null_object_id() : parent()
                {
                }
            }; // null_object_id

            /// Each new object of the class gets a unique, sequential
            /// id.
            template <typename ObjectType>
            class seq_object_id : public object_id_base<ObjectType>
            {
                typedef object_id_base<ObjectType> parent;

            public:
                typedef typename parent::raw_id_type raw_id_type;

                seq_object_id() : parent(s_counter.fetch_add(1, hetcompute::mem_order_relaxed))
                {
                }

                seq_object_id(seq_object_id const& other) : parent(other.get_raw_value())
                {
                }

                // %mint: pause
                seq_object_id(null_object_id<ObjectType> const&) : parent()
                {
                }
                // %mint: resume

            private:
                static std::atomic<raw_id_type> s_counter;
            }; // seq_object_id

            template <typename ObjectType>
            std::atomic<typename object_id_base<ObjectType>::raw_id_type>
                seq_object_id<ObjectType>::s_counter(object_id_base<ObjectType>::s_first_valid_object_id);

        }; // hetcompute::internal::log
    };     // hetcompute::internal
};         // hetcompute
