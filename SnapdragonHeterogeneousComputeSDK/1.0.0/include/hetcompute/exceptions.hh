/** @file exceptions.hh */
#pragma once
#include <exception>
#include <stdint.h>
#include <sstream>
#include <typeinfo>
#include <vector>
#include <hetcompute/internal/compat/compiler_compat.h>
#include <hetcompute/internal/util/strprintf.hh>

namespace hetcompute
{
    /** @addtogroup exceptions
    @{ */
    /**
     *   Superclass of all Qualcomm HetCompute-generated exceptions.
     */
    class hetcompute_exception : public std::exception
    {
    public:
        /** Destructor. */
        virtual ~hetcompute_exception() HETCOMPUTE_NOEXCEPT
        {
        }

        /**
         *   Returns exception description.
         *   @returns
         *   C string describing the exception.
         */
        virtual const char* what() const HETCOMPUTE_NOEXCEPT = 0;
    };

    /** Superclass of all HETCOMPUTE-generated exceptions that
     *  indicate internal or programmer errors.
     */
    class error_exception : public hetcompute_exception
    {
    public:
        error_exception(std::string msg, const char* filename, int lineno, const char* fname);

        ~error_exception() HETCOMPUTE_NOEXCEPT{};

        virtual const char* message() const HETCOMPUTE_NOEXCEPT
        {
            return _message.c_str();
        }

        virtual const char* what() const HETCOMPUTE_NOEXCEPT
        {
            return _long_message.c_str();
        }

        virtual const char* file() const HETCOMPUTE_NOEXCEPT
        {
            return _file.c_str();
        }

        virtual int         line() const HETCOMPUTE_NOEXCEPT
        {
            return _line;
        }

        virtual const char* function() const HETCOMPUTE_NOEXCEPT
        {
            return _function.c_str();
        }

        virtual const char* type() const HETCOMPUTE_NOEXCEPT
        {
            return _classname.c_str();
        }
    private:
        std::string _message;
        std::string _long_message;
        std::string _file;
        int         _line;
        std::string _function;
        std::string _classname;
    };

    /**
     *  Represents a misuse of the Qualcomm HetCompute API. For example, invalid values
     *  passed to a function. Should cause termination of the application
     *  (future releases will behave differently).
     */
    class api_exception : public error_exception
    {
    public:
        api_exception(std::string msg, const char* filename, int lineno, const char* funcname)
            : error_exception(msg, filename, lineno, funcname)
        {
        }
    };

/** @cond PRIVATE*/
// HETCOMPUTE_API_THROW
//  - Throws api_exception.
//  - Enabled by default
//  - See debug.hh for macros that control how this is enabled
// Used for reporting invalid arguments passed in or returned via the API.

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
#define HETCOMPUTE_API_THROW(cond, format, ...)                                                                                              \
    do                                                                                                                                     \
    {                                                                                                                                      \
        if (!(cond))                                                                                                                       \
        {                                                                                                                                  \
            throw ::hetcompute::api_exception(::hetcompute::internal::strprintf(format, ##__VA_ARGS__), __FILE__, __LINE__, __FUNCTION__);     \
        }                                                                                                                                  \
    } while (false)
#else
#define HETCOMPUTE_API_THROW(cond, format, ...)                                                                                              \
    do                                                                                                                                     \
    {                                                                                                                                      \
        if (!(cond))                                                                                                                       \
        {                                                                                                                                  \
            HETCOMPUTE_EXIT_FATAL("API assert [%s] - " format, HETCOMPUTE_API_ASSERT_COND(cond), ##__VA_ARGS__);                           \
        }                                                                                                                                  \
    } while (false)

#endif

// HETCOMPUTE_API_THROW_CUSTOM
//  - Throws the custom exception
//  - Enabled by default
//  - See debug.hh for macros that control how this is enabled
// Used for reporting invalid arguments passed in or returned via the API.
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
#define HETCOMPUTE_API_THROW_CUSTOM(cond, exception, format, ...)                                                                            \
    do                                                                                                                                     \
    {                                                                                                                                      \
        if (!(cond))                                                                                                                       \
        {                                                                                                                                  \
            throw exception(::hetcompute::internal::strprintf(format, ##__VA_ARGS__));                                                       \
        }                                                                                                                                  \
    } while (false)
#else
#define HETCOMPUTE_API_THROW_CUSTOM(cond, exception, format, ...)                                                                            \
    do                                                                                                                                     \
    {                                                                                                                                      \
        if (!(cond))                                                                                                                       \
        {                                                                                                                                  \
            HETCOMPUTE_EXIT_FATAL("API assert [%s] - " format, HETCOMPUTE_API_ASSERT_COND(cond), ##__VA_ARGS__);                           \
        }                                                                                                                                  \
    } while (false)

#endif
/** @endcond */

    /**
     *   Indicates that the thread TLS has been misused or become
     *   corrupted. Should cause termination of the application.
     */
    class tls_exception : public error_exception
    {
    public:
        tls_exception(std::string msg, const char* filename, int lineno, const char* funcname)
            : error_exception(msg, filename, lineno, funcname)
        {
        }
    };

    /**
     *   Exception thrown to abort the current task.
     *
     *   @sa hetcompute::abort_on_cancel()
     *   @sa hetcompute::abort_task()
     */
    class abort_task_exception : public hetcompute_exception
    {
    public:
        explicit abort_task_exception(std::string msg = "aborted task") : _msg(msg)
        {
        }

        virtual ~abort_task_exception() HETCOMPUTE_NOEXCEPT
        {
        }

        virtual const char* what() const HETCOMPUTE_NOEXCEPT
        {
            return _msg.c_str();
        }
    private:
        std::string _msg;
    };

    /**
     * Exception thrown to indicate that a task/group was canceled.
     * Thrown at points-of-use such as wait_for, copy_value, and move_value.
     *
     * @par Example
     * @code
     * auto t = hetcompute::create_task([]{...});
     * t->cancel();
     * t->launch();
     * try {
     *   // Point-of-use of task t
     *   t->wait_for();
     * } catch (const hetcompute::canceled_exception&) {
     *   // Do something
     * } catch (...) {
     *   // Not reached
     * }
     * @endcode
     */
    class canceled_exception : public hetcompute_exception
    {
    public:
        canceled_exception()
        {
        }

        virtual ~canceled_exception() HETCOMPUTE_NOEXCEPT
        {
        }

        HETCOMPUTE_DEFAULT_METHOD(canceled_exception(const canceled_exception&));
        HETCOMPUTE_DEFAULT_METHOD(canceled_exception(canceled_exception&&));
        HETCOMPUTE_DEFAULT_METHOD(canceled_exception& operator=(const canceled_exception&) &);
        HETCOMPUTE_DEFAULT_METHOD(canceled_exception& operator=(canceled_exception&&) &);

        virtual const char* what() const HETCOMPUTE_NOEXCEPT
        {
            return "canceled exception";
        }
    };

    /**
     * Aggregate exception encapsulating all exceptions thrown in a task graph
     * leading up to a point-of-use of a task/group; e.g., wait_for, copy_value,
     * move_value
     *
     * @par Example
     * @code
     * auto g = hetcompute::create_group();
     * g->hetcompute::launch([]{
     *   std::string().at(1); // throws std::out_of_range exception
     * });
     * g->hetcompute::launch([]{
     *   std::string().at(1); // throws std::out_of_range exception
     * });
     * try {
     *   // Point-of-use of group g
     *   g->wait_for();
     * } catch (hetcompute::aggregate_exception& e) {
     *   while (e.has_next()) {
     *     try {
     *       e.next(); // throws
     *     } catch (const std::out_of_range&) {
     *       // Do something
     *     } catch(...) {
     *       // Not reached
     *     }
     *   }
     * } catch (...) {
     *   // Not reached
     * }
     * @endcode
     */
    class aggregate_exception : public hetcompute_exception
    {
    public:
        explicit aggregate_exception(std::vector<std::exception_ptr>* exceptions) : _exceptions(exceptions), _count(exceptions->size())
        {
        }

        aggregate_exception(const aggregate_exception& other)
            : hetcompute_exception(other), _exceptions(other._exceptions), _count(other._count)
        {
        }

        aggregate_exception(aggregate_exception&& other)
            : hetcompute_exception(std::move(other)), _exceptions(other._exceptions), _count(other._count)
        {
        }

        aggregate_exception& operator=(const aggregate_exception& other)
        {
            hetcompute_exception::operator=(other);
            _exceptions                 = other._exceptions;
            _count                      = other._count;
            return *this;
        }

        aggregate_exception& operator=(aggregate_exception&& other)
        {
            hetcompute_exception::operator=(std::move(other));
            _exceptions                 = std::move(other._exceptions);
            _count                      = other._count;
            return *this;
        }

        virtual ~aggregate_exception() HETCOMPUTE_NOEXCEPT
        {
        }

        virtual const char* what() const HETCOMPUTE_NOEXCEPT
        {
            return "aggregate exception";
        }

        /**
         * Returns whether the aggregate_exception contains more exceptions
         * @return true if it contains more exceptions, false otherwise
         */
        bool has_next() const
        {
            return _count > 0;
        }

        /**
         * Throws the next exception contained in the aggregate_exception.
         *
         * @note Does nothing if there are no more exceptions to be thrown.
         */
        void next()
        {
            if (_count-- != 0)
            {
                std::rethrow_exception(_exceptions->at(_count));
            }
        }

    private:
        std::vector<std::exception_ptr>* _exceptions;
        size_t                           _count;
    };

    /**
     * Thrown by the HetCompute runtime if there is a problem with a GPU kernel.
     */
    class gpu_exception : public hetcompute_exception
    {
    public:
        gpu_exception()
        {
        }

        virtual ~gpu_exception() HETCOMPUTE_NOEXCEPT
        {
        }

        HETCOMPUTE_DEFAULT_METHOD(gpu_exception(const gpu_exception&));
        HETCOMPUTE_DEFAULT_METHOD(gpu_exception(gpu_exception&&));
        HETCOMPUTE_DEFAULT_METHOD(gpu_exception& operator=(const gpu_exception&) &);
        HETCOMPUTE_DEFAULT_METHOD(gpu_exception& operator=(gpu_exception&&) &);

        virtual const char* what() const HETCOMPUTE_NOEXCEPT
        {
            return "gpu exception";
        }
    };

    /**
     * Thrown by the HetCompute runtime if there is a problem with a DSP kernel.
     */
    class dsp_exception : public hetcompute_exception
    {
    public:
        dsp_exception()
        {
        }

        virtual ~dsp_exception() HETCOMPUTE_NOEXCEPT
        {
        }

        HETCOMPUTE_DEFAULT_METHOD(dsp_exception(const dsp_exception&));
        HETCOMPUTE_DEFAULT_METHOD(dsp_exception(dsp_exception&&));
        HETCOMPUTE_DEFAULT_METHOD(dsp_exception& operator=(const dsp_exception&) &);
        HETCOMPUTE_DEFAULT_METHOD(dsp_exception& operator=(dsp_exception&&) &);

        virtual const char* what() const HETCOMPUTE_NOEXCEPT
        {
            return "dsp exception";
        }
    };

    /** @} */ /* end_addtogroup exceptions */

    /*
     * API to check if application wants exceptions to be
     * enabled (or) disabled in the library.
     */
    bool isAPIExceptionEnabled();

    /*
     * API to set if application wants exception to be
     * enabled (or) disabled in the library
     */
    void setAPIExceptionEnable(bool enable);

}; // namespace hetcompute
