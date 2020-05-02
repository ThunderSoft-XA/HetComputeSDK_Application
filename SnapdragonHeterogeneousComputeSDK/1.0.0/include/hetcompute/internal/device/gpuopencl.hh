#pragma once

#ifdef HETCOMPUTE_HAVE_OPENCL

#include <hetcompute/internal/util/debug.hh>

HETCOMPUTE_GCC_IGNORE_BEGIN("-Wshadow");
HETCOMPUTE_GCC_IGNORE_BEGIN("-Wdeprecated-declarations");
HETCOMPUTE_GCC_IGNORE_BEGIN("-Weffc++");
HETCOMPUTE_GCC_IGNORE_BEGIN("-Wold-style-cast");
HETCOMPUTE_GCC_IGNORE_BEGIN("-Wunused-parameter");
HETCOMPUTE_GCC_IGNORE_BEGIN("-Wunused-function");
HETCOMPUTE_GCC_IGNORE_BEGIN("-Wmissing-braces");

// %mint: pause
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
#define __CL_ENABLE_EXCEPTIONS
#endif

// %mint: resume
#if defined(__APPLE__) || defined(__MACOSX)
#include <OpenCL/cl.hpp>
#include <OpenCL/cl_ext_qcom.h>
#else
#include <CL/cl.h>
#include <CL/cl_ext_qcom.h>

#ifdef HETCOMPUTE_HAVE_OPENCL_2_0
// To enable OpenCL 2.0 features, two #defines should be in place at this point:
//    - HETCOMPUTE_HAVE_OPENCL == 1 -----  pulled in from cmake config
//    - CL_VERSION_2_0   == 1 -----  pulled in from cl.h
// Therefore we put sanity checks here.
#if !(HETCOMPUTE_HAVE_OPENCL == 1)
#error WITH_OPENCL=1 is required to enable OpenCL 2.x features.
#endif
#if !(CL_VERSION_2_0 == 1)
#error Using a CL1.x header for CL 2.x target.
#endif

#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_ENABLE_SIZE_T_COMPATIBILITY                          // support size_t in CL1.x
#define CL_HPP_ENABLE_PROGRAM_CONSTRUCTION_FROM_ARRAY_COMPATIBILITY // support program in CL1.x
#define CL_HPP_TARGET_OPENCL_VERSION 200
#define HETCOMPUTE_CL_TO_CL2 // Enable creating cl2_arena instead of cl_arena in buffers
#include <CL/cl2.hpp>
#else
#define CL_HPP_TARGET_OPENCL_VERSION 120 // default CL version even if the header is 2.0
#define CL_HPP_MINIMUM_OPENCL_VERSION 100
#define CL_HPP_CL_1_2_DEFAULT_BUILD // default to OpenCL 1.2 compilation
#include <CL/cl.hpp>
#endif // HETCOMPUTE_HAVE_OPENCL_2_0

#endif // Apple

#ifdef HETCOMPUTE_HAVE_OPENCL_2_0
#define HETCOMPUTE_CL_VECTOR_CLASS cl::vector
#define HETCOMPUTE_CL_STRING_CLASS cl::string
#else
#include <vector>
#include <string>
#define HETCOMPUTE_CL_VECTOR_CLASS std::vector
#define HETCOMPUTE_CL_STRING_CLASS std::string
#endif // HETCOMPUTE_HAVE_OPENCL_2_0

HETCOMPUTE_GCC_IGNORE_END("-Wmissing-braces");
HETCOMPUTE_GCC_IGNORE_END("-Wunused-function");
HETCOMPUTE_GCC_IGNORE_END("-Wunused-parameter");
HETCOMPUTE_GCC_IGNORE_END("-Wold-style-cast");
HETCOMPUTE_GCC_IGNORE_END("-Weffc++");
HETCOMPUTE_GCC_IGNORE_END("-Wdeprecated-declarations");
HETCOMPUTE_GCC_IGNORE_END("-Wshadow");
namespace hetcompute
{
    namespace internal
    {
        const char* get_cl_error_string(cl_int error);

    }; // namespace internal
};  // namespace hetcompute

#endif // HETCOMPUTE_HAVE_OPENCL
