#pragma once

#include <hetcompute/exceptions.hh>

namespace hetcompute
{
    namespace internal
    {
        /**
         *  Represents an Qualcomm HetCompute API assertion.  Since catching
         *  ASSERTs in our tests is not practical (forking doesn't work
         *  with the threadpool) we convert HETCOMPUTE_API_ASSERT into exceptions that can
         *  be thrown under the HETCOMPUTE_THROW_ON_API_ASSERT flag. This is used
         *  only internally for tests.
        */
        class api_assert_exception : public error_exception
        {
        public:
            api_assert_exception(std::string msg, const char* filename, int lineno, const char* funcname)
                : error_exception(msg, filename, lineno, funcname)
            {
            }
        };

        /**
         *  Represents an internal Qualcomm HetCompute assertion.  Since catching
         *  ASSERTs in our tests is not practical (forking doesn't work
         *  with the threadpool) we convert HETCOMPUTE_INTERNAL_ASSERT into
         *  exceptions that can be thrown under the HETCOMPUTE_THROW_ON_API_ASSERT
         *  flag. This is used only internally for tests.
        */
        class assert_exception : public error_exception
        {
        public:
            assert_exception(std::string msg, const char* filename, int lineno, const char* funcname)
                : error_exception(msg, filename, lineno, funcname)
            {
            }
        };

        /**
         *  Represents an fatal error.  Since catching
         *  ASSERTs in our tests is not practical (forking doesn't work
         *  with the threadpool) we convert HETCOMPUTE_FATAL into
         *  exceptions that can be thrown under the HETCOMPUTE_THROW_ON_API_ASSERT
         *  flag. This is used only internally for tests.
        */
        class fatal_exception : public error_exception
        {
        public:
            fatal_exception(std::string msg, const char* filename, int lineno, const char* funcname)
                : error_exception(msg, filename, lineno, funcname)
            {
            }
        };

    };  // namespace internal
};  // namespace hetcompute
