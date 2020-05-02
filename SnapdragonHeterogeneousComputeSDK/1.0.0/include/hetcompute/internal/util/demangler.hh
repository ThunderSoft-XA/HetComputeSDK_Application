#pragma once

// Include std headers
#include <string>

#ifdef HETCOMPUTE_HAVE_RTTI
#include <typeinfo>
#endif

#ifdef _MSC_VER
#include <type_traits>
#endif // _MSC_VER

// Include internal headers
#include <hetcompute/internal/util/macros.hh>

namespace hetcompute
{
    namespace internal
    {
        /**
         * The demangler struct contains methods that make it easy
         * for the programmer to get a string out of one or more
         * types.
         *
         * HETCOMPUTE_HAVE_RTTI must be enabled for it to work.
         *
         * You can't instantiate a demangler object. We opted for
         * a structure instead of namespace because of the methods
         * that deal with variadic templates. We didn't want
         * to expose them
        */
        class demangler
        {
        public:
            /**
             * Demangles type_info variables.
             * @param type Type to demangle.
             * @return
             * std::string -- Textual representation of the type.
             */
#ifdef HETCOMPUTE_HAVE_RTTI
            static std::string demangle(std::type_info const& type)
            {
                return do_work(type);
            }
#endif

            /**
             * Demangles type_info variables.
             * @return
             * std::string -- Textual representation of the type.
             */
            template <typename... TS>
            static std::string demangle()
            {
#ifdef HETCOMPUTE_HAVE_RTTI
                std::string description(" ");
                demangle_traverse_types<TS...>(description, typename std::integral_constant<bool, (sizeof...(TS) != 0)>::type());
                return description;
#else
                return std::string("Enable RTTI for demangling class names.");
#endif
            }

        private:
            /**
             * Check whether we have arguments to demangle
             */
            template <typename... Types>
            static void demangle_traverse_types(std::string& description, std::true_type)
            {
                demangle_traverse_types_imp<Types...>(description);
            }

            template <typename... Types>
            static void demangle_traverse_types(std::string&, std::false_type)
            {
            }

            /**
             * Calls do_work on each of the types in the template.
             * Does nothing if rtti is disabled.
             * @param description String that will describe all types
             *
            */
            template <typename S1, typename S2, typename... Rest>
            static void demangle_traverse_types_imp(std::string& description)
            {
#ifdef HETCOMPUTE_HAVE_RTTI
                description += do_work(typeid(S1));
                description += ",";
                demangle_traverse_types_imp<S2, Rest...>(description);
#endif
                HETCOMPUTE_UNUSED(description);
            }

            /**
             * Calls do_work on the last (or only type in the template)
             * @param description String that will describe all types.
             */
            template <typename S1>
            static void demangle_traverse_types_imp(std::string& description)
            {
#ifdef HETCOMPUTE_HAVE_RTTI
                description += do_work(typeid(S1));
#endif
                HETCOMPUTE_UNUSED(description);
            }

#ifdef HETCOMPUTE_HAVE_RTTI
            // Does the actual demangling work.
            // Demangles type_info variables.
            // @param type Type to demangle.
            // @return
            // std::string -- Textual representation of the type.
            static std::string do_work(std::type_info const& type);
#endif

            // Disable all construction, copying and movement.
            HETCOMPUTE_DELETE_METHOD(demangler());
            HETCOMPUTE_DELETE_METHOD(demangler(demangler const&));
            HETCOMPUTE_DELETE_METHOD(demangler(demangler&&));
            HETCOMPUTE_DELETE_METHOD(demangler& operator=(demangler const&));
            HETCOMPUTE_DELETE_METHOD(demangler& operator=(demangler&&));
        };

    }; // namespace internal
};     // namespace hetcompute
