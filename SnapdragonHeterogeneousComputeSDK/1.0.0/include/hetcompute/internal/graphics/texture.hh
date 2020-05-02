#pragma once

#ifdef HETCOMPUTE_HAVE_OPENCL

// Include std header first
#include <set>
#include <unordered_set>

// Include user visible headers
#include <hetcompute/memregion.hh>
#include <hetcompute/texturetype.hh>

// Include internal headers last
#include <hetcompute/internal/buffer/memregionaccessor.hh>
#include <hetcompute/internal/device/cldevice.hh>
#include <hetcompute/internal/device/gpuopencl.hh>
#include <hetcompute/internal/graphics/texture_init.hh>

#ifdef HETCOMPUTE_HAVE_QTI_DSP
// #include <rpcmem.h> -- we just use rpcmem_to_fd, so we declare the prototype
extern "C" int rpcmem_to_fd(void*);
#endif // HETCOMPUTE_HAVE_QTI_DSP

namespace hetcompute
{
    namespace graphics
    {
        namespace internal
        {
            struct cl_StandardFormat
            {
                static constexpr bool extended_format = false;
            };

            struct cl_ExtendedFormat
            {
                static constexpr bool extended_format = true;
            };

            struct cl_RGBAsnorm_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::RGBAsnorm_int8;
                enum
                {
                    order = CL_RGBA
                };
                enum
                {
                    type = CL_SNORM_INT8
                };
            };

            struct cl_RGBAunorm_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::RGBAunorm_int8;
                enum
                {
                    order = CL_RGBA
                };
                enum
                {
                    type = CL_UNORM_INT8
                };
            };

            struct cl_RGBAsigned_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::RGBAsigned_int8;
                enum
                {
                    order = CL_RGBA
                };
                enum
                {
                    type = CL_SIGNED_INT8
                };
            };

            struct cl_RGBAunsigned_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::RGBAunsigned_int8;
                enum
                {
                    order = CL_RGBA
                };
                enum
                {
                    type = CL_UNSIGNED_INT8
                };
            };

            struct cl_RGBAunorm_int16 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::RGBAunorm_int16;
                enum
                {
                    order = CL_RGBA
                };
                enum
                {
                    type = CL_UNORM_INT16
                };
            };

            struct cl_RGBA_float : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::RGBA_float;
                enum
                {
                    order = CL_RGBA
                };
                enum
                {
                    type = CL_FLOAT
                };
            };

            struct cl_RGBA_half : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::RGBA_half;
                enum
                {
                    order = CL_RGBA
                };
                enum
                {
                    type = CL_HALF_FLOAT
                };
            };

            struct cl_ARGBsnorm_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::ARGBsnorm_int8;
                enum
                {
                    order = CL_ARGB
                };
                enum
                {
                    type = CL_SNORM_INT8
                };
            };

            struct cl_ARGBunorm_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::ARGBunorm_int8;
                enum
                {
                    order = CL_ARGB
                };
                enum
                {
                    type = CL_UNORM_INT8
                };
            };

            struct cl_ARGBsigned_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::ARGBsigned_int8;
                enum
                {
                    order = CL_ARGB
                };
                enum
                {
                    type = CL_SIGNED_INT8
                };
            };

            struct cl_ARGBunsigned_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::ARGBunsigned_int8;
                enum
                {
                    order = CL_ARGB
                };
                enum
                {
                    type = CL_UNSIGNED_INT8
                };
            };

            struct cl_BGRAsnorm_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::BGRAsnorm_int8;
                enum
                {
                    order = CL_BGRA
                };
                enum
                {
                    type = CL_SNORM_INT8
                };
            };

            struct cl_BGRAunorm_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::BGRAunorm_int8;
                enum
                {
                    order = CL_BGRA
                };
                enum
                {
                    type = CL_UNORM_INT8
                };
            };

            struct cl_BGRAsigned_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::BGRAsigned_int8;
                enum
                {
                    order = CL_BGRA
                };
                enum
                {
                    type = CL_SIGNED_INT8
                };
            };

            struct cl_BGRAunsigned_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::BGRAunsigned_int8;
                enum
                {
                    order = CL_BGRA
                };
                enum
                {
                    type = CL_UNSIGNED_INT8
                };
            };

            struct cl_RGsnorm_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::RGsnorm_int8;
                enum
                {
                    order = CL_RG
                };
                enum
                {
                    type = CL_SNORM_INT8
                };
            };

            struct cl_RGunorm_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::RGunorm_int8;
                enum
                {
                    order = CL_RG
                };
                enum
                {
                    type = CL_UNORM_INT8
                };
            };

            struct cl_RGsigned_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::RGsigned_int8;
                enum
                {
                    order = CL_RG
                };
                enum
                {
                    type = CL_SIGNED_INT8
                };
            };

            struct cl_RGunsigned_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::RGunsigned_int8;
                enum
                {
                    order = CL_RG
                };
                enum
                {
                    type = CL_UNSIGNED_INT8
                };
            };

            struct cl_RGunorm_int16 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::RGunorm_int16;
                enum
                {
                    order = CL_RG
                };
                enum
                {
                    type = CL_UNORM_INT16
                };
            };

            struct cl_RG_float : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::RG_float;
                enum
                {
                    order = CL_RG
                };
                enum
                {
                    type = CL_FLOAT
                };
            };

            struct cl_RG_half : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::RG_half;
                enum
                {
                    order = CL_RG
                };
                enum
                {
                    type = CL_HALF_FLOAT
                };
            };

            struct cl_INTENSITYsnorm_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::INTENSITYsnorm_int8;
                enum
                {
                    order = CL_INTENSITY
                };
                enum
                {
                    type = CL_SNORM_INT8
                };
            };

            struct cl_INTENSITYunorm_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::INTENSITYunorm_int8;
                enum
                {
                    order = CL_INTENSITY
                };
                enum
                {
                    type = CL_UNORM_INT8
                };
            };

            struct cl_INTENSITYsnorm_int16 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::INTENSITYsnorm_int16;
                enum
                {
                    order = CL_INTENSITY
                };
                enum
                {
                    type = CL_SNORM_INT16
                };
            };

            struct cl_INTENSITYunorm_int16 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::INTENSITYunorm_int16;
                enum
                {
                    order = CL_INTENSITY
                };
                enum
                {
                    type = CL_UNORM_INT16
                };
            };

            struct cl_INTENSITY_float : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::INTENSITY_float;
                enum
                {
                    order = CL_INTENSITY
                };
                enum
                {
                    type = CL_FLOAT
                };
            };

            struct cl_LUMINANCEsnorm_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::LUMINANCEsnorm_int8;
                enum
                {
                    order = CL_LUMINANCE
                };
                enum
                {
                    type = CL_SNORM_INT8
                };
            };

            struct cl_LUMINANCEunorm_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::LUMINANCEunorm_int8;
                enum
                {
                    order = CL_LUMINANCE
                };
                enum
                {
                    type = CL_UNORM_INT8
                };
            };

            struct cl_LUMINANCEsnorm_int16 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::LUMINANCEsnorm_int16;
                enum
                {
                    order = CL_LUMINANCE
                };
                enum
                {
                    type = CL_SNORM_INT16
                };
            };

            struct cl_LUMINANCEunorm_int16 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::LUMINANCEunorm_int16;
                enum
                {
                    order = CL_LUMINANCE
                };
                enum
                {
                    type = CL_UNORM_INT16
                };
            };

            struct cl_LUMINANCE_float : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::LUMINANCE_float;
                enum
                {
                    order = CL_LUMINANCE
                };
                enum
                {
                    type = CL_FLOAT
                };
            };

            struct cl_Rsnorm_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::Rsnorm_int8;
                enum
                {
                    order = CL_R
                };
                enum
                {
                    type = CL_SNORM_INT8
                };
            };

            struct cl_Runorm_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::Runorm_int8;
                enum
                {
                    order = CL_R
                };
                enum
                {
                    type = CL_UNORM_INT8
                };
            };

            struct cl_Rsigned_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::Rsigned_int8;
                enum
                {
                    order = CL_R
                };
                enum
                {
                    type = CL_SIGNED_INT8
                };
            };

            struct cl_Runsigned_int8 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::Runsigned_int8;
                enum
                {
                    order = CL_R
                };
                enum
                {
                    type = CL_UNSIGNED_INT8
                };
            };

            struct cl_Runorm_int16 : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::Runorm_int16;
                enum
                {
                    order = CL_R
                };
                enum
                {
                    type = CL_UNORM_INT16
                };
            };

            struct cl_R_float : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::R_float;
                enum
                {
                    order = CL_R
                };
                enum
                {
                    type = CL_FLOAT
                };
            };

            struct cl_R_half : cl_StandardFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::R_half;
                enum
                {
                    order = CL_R
                };
                enum
                {
                    type = CL_HALF_FLOAT
                };
            };

            struct cl_NV12unorm_int8 : cl_ExtendedFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::NV12unorm_int8;
                enum
                {
                    order    = CL_QCOM_NV12,
                    order_y  = CL_QCOM_NV12_Y,
                    order_uv = CL_QCOM_NV12_UV
                };
                enum
                {
                    type = CL_UNORM_INT8
                };
            };

            struct cl_P010unorm_int10 : cl_ExtendedFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::P010unorm_int10;
                enum
                {
                    order    = CL_QCOM_P010,
                    order_y  = CL_QCOM_P010_Y,
                    order_uv = CL_QCOM_P010_UV
                };
                enum
                {
                    type = CL_QCOM_UNORM_INT10
                };
            };

            struct cl_TP10unorm_int10 : cl_ExtendedFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::TP10unorm_int10;
                enum
                {
                    order    = CL_QCOM_TP10,
                    order_y  = CL_QCOM_TP10_Y,
                    order_uv = CL_QCOM_TP10_UV
                };
                enum
                {
                    type = CL_QCOM_UNORM_INT10
                };
            };

            struct cl_TiledNV12unorm_int8 : cl_ExtendedFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::TiledNV12unorm_int8;
                enum
                {
                    order    = CL_QCOM_TILED_NV12,
                    order_y  = CL_QCOM_TILED_NV12_Y,
                    order_uv = CL_QCOM_TILED_NV12_UV
                };
                enum
                {
                    type = CL_UNORM_INT8
                };
            };

            struct cl_TiledP010unorm_int10 : cl_ExtendedFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::TiledP010unorm_int10;
                enum
                {
                    order    = CL_QCOM_TILED_P010,
                    order_y  = CL_QCOM_TILED_P010_Y,
                    order_uv = CL_QCOM_TILED_P010_UV
                };
                enum
                {
                    type = CL_QCOM_UNORM_INT10
                };
            };

            struct cl_TiledTP10unorm_int10 : cl_ExtendedFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::TiledTP10unorm_int10;
                enum
                {
                    order    = CL_QCOM_TILED_TP10,
                    order_y  = CL_QCOM_TILED_TP10_Y,
                    order_uv = CL_QCOM_TILED_TP10_UV
                };
                enum
                {
                    type = CL_QCOM_UNORM_INT10
                };
            };

            struct cl_CompressedNV12unorm_int8 : cl_ExtendedFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::CompressedNV12unorm_int8;
                enum
                {
                    order    = CL_QCOM_COMPRESSED_NV12,
                    order_y  = CL_QCOM_COMPRESSED_NV12_Y,
                    order_uv = CL_QCOM_COMPRESSED_NV12_UV
                };
                enum
                {
                    type = CL_UNORM_INT8
                };
            };

            struct cl_CompressedNV124Runorm_int8 : cl_ExtendedFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::CompressedNV124Runorm_int8;
                enum
                {
                    order    = CL_QCOM_COMPRESSED_NV12_4R,
                    order_y  = CL_QCOM_COMPRESSED_NV12_4R_Y,
                    order_uv = CL_QCOM_COMPRESSED_NV12_4R_UV
                };
                enum
                {
                    type = CL_UNORM_INT8
                };
            };

            struct cl_CompressedP010unorm_int10 : cl_ExtendedFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::CompressedP010unorm_int10;
                enum
                {
                    order    = CL_QCOM_COMPRESSED_P010,
                    order_y  = CL_QCOM_COMPRESSED_P010_Y,
                    order_uv = CL_QCOM_COMPRESSED_P010_UV
                };
                enum
                {
                    type = CL_QCOM_UNORM_INT10
                };
            };

            struct cl_CompressedTP10unorm_int10 : cl_ExtendedFormat
            {
                static constexpr image_format hetcompute_image_format = image_format::CompressedTP10unorm_int10;
                enum
                {
                    order    = CL_QCOM_COMPRESSED_TP10,
                    order_y  = CL_QCOM_COMPRESSED_TP10_Y,
                    order_uv = CL_QCOM_COMPRESSED_TP10_UV
                };
                enum
                {
                    type = CL_QCOM_UNORM_INT10
                };
            };

            template <cl_filter_mode mode>
            struct cl_filter_val
            {
                enum
                {
                    cl_val = mode
                };
            };

            template <filter_mode mode>
            struct translate_filter_mode
            {
            };

            template <>
            struct translate_filter_mode<filter_mode::FILTER_NEAREST> : cl_filter_val<CL_FILTER_NEAREST>
            {
            };

            template <>
            struct translate_filter_mode<filter_mode::FILTER_LINEAR> : cl_filter_val<CL_FILTER_LINEAR>
            {
            };

            template <cl_addressing_mode mode>
            struct cl_addr_val
            {
                enum
                {
                    cl_val = mode
                };
            };

            template <addressing_mode mode>
            struct translate_addressing_mode
            {
            };

            template <>
            struct translate_addressing_mode<addressing_mode::ADDRESS_NONE> : cl_addr_val<CL_ADDRESS_NONE>
            {
            };

            template <>
            struct translate_addressing_mode<addressing_mode::ADDRESS_CLAMP_TO_EDGE> : cl_addr_val<CL_ADDRESS_CLAMP_TO_EDGE>
            {
            };

            template <>
            struct translate_addressing_mode<addressing_mode::ADDRESS_CLAMP> : cl_addr_val<CL_ADDRESS_CLAMP>
            {
            };

            template <>
            struct translate_addressing_mode<addressing_mode::ADDRESS_REPEAT> : cl_addr_val<CL_ADDRESS_REPEAT>
            {
            };

            template <typename T>
            struct type_wrapper
            {
                typedef T type;
            };

            template <image_format img_format>
            struct translate_image_format_to_struct
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::RGBAsnorm_int8> : type_wrapper<cl_RGBAsnorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::RGBAunorm_int8> : type_wrapper<cl_RGBAunorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::RGBAsigned_int8> : type_wrapper<cl_RGBAsigned_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::RGBAunsigned_int8> : type_wrapper<cl_RGBAunsigned_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::RGBAunorm_int16> : type_wrapper<cl_RGBAunorm_int16>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::RGBA_float> : type_wrapper<cl_RGBA_float>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::RGBA_half> : type_wrapper<cl_RGBA_half>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::ARGBsnorm_int8> : type_wrapper<cl_ARGBsnorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::ARGBunorm_int8> : type_wrapper<cl_ARGBunorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::ARGBsigned_int8> : type_wrapper<cl_ARGBsigned_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::ARGBunsigned_int8> : type_wrapper<cl_ARGBunsigned_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::BGRAsnorm_int8> : type_wrapper<cl_BGRAsnorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::BGRAunorm_int8> : type_wrapper<cl_BGRAunorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::BGRAsigned_int8> : type_wrapper<cl_BGRAsigned_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::BGRAunsigned_int8> : type_wrapper<cl_BGRAunsigned_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::RGsnorm_int8> : type_wrapper<cl_RGsnorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::RGunorm_int8> : type_wrapper<cl_RGunorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::RGsigned_int8> : type_wrapper<cl_RGsigned_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::RGunsigned_int8> : type_wrapper<cl_RGunsigned_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::RGunorm_int16> : type_wrapper<cl_RGunorm_int16>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::RG_float> : type_wrapper<cl_RG_float>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::RG_half> : type_wrapper<cl_RG_half>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::INTENSITYsnorm_int8> : type_wrapper<cl_INTENSITYsnorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::INTENSITYunorm_int8> : type_wrapper<cl_INTENSITYunorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::INTENSITYsnorm_int16> : type_wrapper<cl_INTENSITYsnorm_int16>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::INTENSITYunorm_int16> : type_wrapper<cl_INTENSITYunorm_int16>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::INTENSITY_float> : type_wrapper<cl_INTENSITY_float>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::LUMINANCEsnorm_int8> : type_wrapper<cl_LUMINANCEsnorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::LUMINANCEunorm_int8> : type_wrapper<cl_LUMINANCEunorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::LUMINANCEsnorm_int16> : type_wrapper<cl_LUMINANCEsnorm_int16>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::LUMINANCEunorm_int16> : type_wrapper<cl_LUMINANCEunorm_int16>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::LUMINANCE_float> : type_wrapper<cl_LUMINANCE_float>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::Rsnorm_int8> : type_wrapper<cl_Rsnorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::Runorm_int8> : type_wrapper<cl_Runorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::Rsigned_int8> : type_wrapper<cl_Rsigned_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::Runsigned_int8> : type_wrapper<cl_Runsigned_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::Runorm_int16> : type_wrapper<cl_Runorm_int16>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::R_float> : type_wrapper<cl_R_float>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::R_half> : type_wrapper<cl_R_half>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::NV12unorm_int8> : type_wrapper<cl_NV12unorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::P010unorm_int10> : type_wrapper<cl_P010unorm_int10>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::TP10unorm_int10> : type_wrapper<cl_TP10unorm_int10>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::TiledNV12unorm_int8> : type_wrapper<cl_TiledNV12unorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::TiledP010unorm_int10> : type_wrapper<cl_TiledP010unorm_int10>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::TiledTP10unorm_int10> : type_wrapper<cl_TiledTP10unorm_int10>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::CompressedNV12unorm_int8> : type_wrapper<cl_CompressedNV12unorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::CompressedNV124Runorm_int8> : type_wrapper<cl_CompressedNV124Runorm_int8>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::CompressedP010unorm_int10> : type_wrapper<cl_CompressedP010unorm_int10>
            {
            };

            template <>
            struct translate_image_format_to_struct<image_format::CompressedTP10unorm_int10> : type_wrapper<cl_CompressedTP10unorm_int10>
            {
            };

            // Defining HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE will use the CL C APIs directly
            // instead of the C++ wrappers.
            #define HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE

            class base_texture_cl
            {
            public:
                ::hetcompute::internal::legacy::device_ptr _device_ptr;
                cl::CommandQueue&                          _cl_cmd_q;

                void* _host_ptr;

#ifdef HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE
                cl_mem _img;
#else  // HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE
                cl::Image _img; // this can handle Image2D and Image3D in c++ binding
#endif // HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE

                void*         _mapped_ptr;
                image_size<3> _base_image_size;

                explicit base_texture_cl(void* host_ptr, image_size<3> base_image_size)
                    : _device_ptr(::hetcompute::internal::s_dev_ptrs->at(0)),
                      _cl_cmd_q(::hetcompute::internal::access_cldevice::get_cl_cmd_queue(_device_ptr)),
                      _host_ptr(host_ptr),
                      _img(),
                      _mapped_ptr(nullptr),
                      _base_image_size(base_image_size)
                {
                }

                ~base_texture_cl()
                {
                    HETCOMPUTE_DLOG("DESTRUCTING texture %p", _img);
#ifdef HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE
                    cl_int status = ::clReleaseMemObject(_img);
                    if (status != CL_SUCCESS)
                    {
                        HETCOMPUTE_FATAL("Failed to release texture %p", _img);
                    }
#endif // HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE
                }

                void unmap()
                {
#ifdef HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE
                    cl_int   status;
                    cl_event evt;
                    status = ::clEnqueueUnmapMemObject(_cl_cmd_q(), _img, _mapped_ptr, 0, nullptr, &evt);
                    clWaitForEvents(1, &evt);

                    HETCOMPUTE_DLOG("texture_cl(): unmap clerror=%d\n", status);
                    cl::detail::errHandler(status, "texture unmap failed");
#endif // HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE

                    HETCOMPUTE_INTERNAL_ASSERT(_mapped_ptr != nullptr, "map not called before unmap");
                }

                void* map() { return map(_base_image_size); }

                void* map(image_size<3>& image_dims)
                {
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                    try
                    {
#endif
                        cl::size_t<3> origin;
                        origin[0] = 0;
                        origin[1] = 0;
                        origin[2] = 0;

                        cl::size_t<3> region;
                        region[0] = image_dims._width;
                        region[1] = image_dims._height;
                        region[2] = image_dims._depth; // 1 for 2D image

                        size_t row_pitch, slice_pitch;

#ifdef HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE
                        cl_int error;
                        HETCOMPUTE_DLOG("cmd_q=%p", _cl_cmd_q());
                        _mapped_ptr = ::clEnqueueMapImage(_cl_cmd_q(),
                                                          _img,
                                                          CL_TRUE,
                                                          CL_MAP_READ | CL_MAP_WRITE,
                                                          origin,
                                                          region,
                                                          &row_pitch,
                                                          &slice_pitch, // ignored for 2D image
                                                          0,
                                                          nullptr,
                                                          nullptr,
                                                          &error);

                        HETCOMPUTE_DLOG("texture_cl(): map clerror=%d\n", error);
                        cl::detail::errHandler(error, "texture map failed");
#else  // HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE
                        _mapped_ptr = _cl_cmd_q.enqueueMapImage(_img,
                                                                CL_TRUE,
                                                                // FIXME: inefficient, but correct
                                                                //(can cause unnecessary copies
                                                                // by host)
                                                                CL_MAP_READ | CL_MAP_WRITE,
                                                                origin,
                                                                region,
                                                                &row_pitch,
                                                                &slice_pitch,
                                                                nullptr,
                                                                nullptr,
                                                                nullptr);
#endif // HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE
                        HETCOMPUTE_INTERNAL_ASSERT(_host_ptr == nullptr || _host_ptr == _mapped_ptr, "_host_ptr and _mapped_ptr mismatch");

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                    }
                    catch (cl::Error& err)
                    {
                        HETCOMPUTE_FATAL("cl::CommandQueue::enqueueMapImage()->%s", ::hetcompute::internal::get_cl_error_string(err.err()));
                    }
                    catch (...)
                    {
                        HETCOMPUTE_FATAL("Unknown error in cl::CommandQueue::enqueueMapImage()");
                    }
#endif

                    return _mapped_ptr;
                } // map()

            };  // class base_texture_cl

            template <image_format img_format, int dims>
            struct texture_cl;

            // hetcompute 1d texture wrapper
            template <image_format img_format>
            struct texture_cl<img_format, 1> : public base_texture_cl, ::hetcompute::internal::ref_counted_object<texture_cl<img_format, 1>>
            {
            private:
                image_size<1> _image_size;
                using image_cl_desc = typename translate_image_format_to_struct<img_format>::type;

            public:
                texture_cl(image_size<1> const& is, bool read_only, void* host_ptr, int fd = -1, bool is_ion = false, bool is_cached = false)
                    : base_texture_cl(host_ptr, { is._width, 1, 1 }), _image_size(is)
                {
                    HETCOMPUTE_API_ASSERT(host_ptr != nullptr, "Currently host_ptr == nullptr not supported. Please pre-allocate.");
                    _host_ptr = host_ptr;

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                    try
                    {
#endif

#ifdef HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE
#ifdef ANDROID
                        HETCOMPUTE_UNUSED(is_cached);
                        HETCOMPUTE_UNUSED(fd);
                        if (!is_ion)
                        {
                            if (image_cl_desc::extended_format) HETCOMPUTE_FATAL("Extended texutre formats are only supported with ION buffers.");

                            cl_image_format format;
                            format.image_channel_order     = image_cl_desc::order;
                            format.image_channel_data_type = image_cl_desc::type;
                            HETCOMPUTE_CLANG_IGNORE_BEGIN("-Wmissing-braces");
                            cl_image_desc image_desc = { CL_MEM_OBJECT_IMAGE1D,
                                                         is._width,
                                                         0, // height
                                                         0, // depth
                                                         0, // array_size
                                                         0, // row_pitch
                                                         0, // slice_pitch
                                                         0, // mip_level
                                                         0, // num_samples
                                                         nullptr };
                            HETCOMPUTE_CLANG_IGNORE_END("-Wmissing-braces");
                            cl_int error;

                            cl::Context& context = ::hetcompute::internal::access_cldevice::get_cl_context(_device_ptr);

                            HETCOMPUTE_GCC_IGNORE_BEGIN("-Wdeprecated-declarations");
                            _img = ::clCreateImage(context(),
                                                   ((_host_ptr == nullptr ? CL_MEM_ALLOC_HOST_PTR : CL_MEM_USE_HOST_PTR) |
                                                    (read_only ? CL_MEM_READ_ONLY : CL_MEM_READ_WRITE)),
                                                   &format,
                                                   &image_desc,
                                                   host_ptr,
                                                   &error);
                            HETCOMPUTE_GCC_IGNORE_END("-Wdeprecated-declarations");

                            HETCOMPUTE_DLOG("texture_cl(): clerror=%d\n", error);
                            cl::detail::errHandler(error, "texture creation failed");
                        }
                        else
                        {
                            HETCOMPUTE_FATAL("hetcompute doesn't support ion 1d texture yet.");
                        }
#else  // ANDROID
                        HETCOMPUTE_UNUSED(is);
                        HETCOMPUTE_UNUSED(read_only);
                        HETCOMPUTE_UNUSED(fd);
                        HETCOMPUTE_UNUSED(is_ion);
                        HETCOMPUTE_UNUSED(is_cached);
#endif // ANDROID
#endif // HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                    }
                    catch (cl::Error& err)
                    {
                        HETCOMPUTE_FATAL("Could not construct cl 1D image in texture_cl()->%s for format: %d",
                                         ::hetcompute::internal::get_cl_error_string(err.err()),
                                         static_cast<int>(img_format));
                    }
#endif
                }

                ~texture_cl() {}

            }; // struct texture_cl<img_format, 1>


            // hetcompute 2d texture wrapper
            template <image_format img_format>
            struct texture_cl<img_format, 2> : public base_texture_cl, ::hetcompute::internal::ref_counted_object<texture_cl<img_format, 2>>
            {
            private:
                image_size<2> _image_size;
                using image_cl_desc = typename translate_image_format_to_struct<img_format>::type;

            public:
                texture_cl(image_size<2> const& is, bool read_only, void* host_ptr, int fd = -1, bool is_ion = false, bool is_cached = false)
                    : base_texture_cl(host_ptr, { is._width, is._height, 1 }), _image_size(is)
                {
                    HETCOMPUTE_API_ASSERT(host_ptr != nullptr, "Currently host_ptr == nullptr not supported. Please pre-allocate.");
                    _host_ptr = host_ptr;

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                    try
                    {
#endif

#ifdef HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE
                        cl_image_format format;
                        format.image_channel_order     = image_cl_desc::order;
                        format.image_channel_data_type = image_cl_desc::type;

                        cl_int error;

                        cl::Context& context = ::hetcompute::internal::access_cldevice::get_cl_context(_device_ptr);

                        HETCOMPUTE_GCC_IGNORE_BEGIN("-Wdeprecated-declarations");
                        if (!is_ion)
                        {
                            if (image_cl_desc::extended_format) HETCOMPUTE_FATAL("Extended texutre formats are only supported with ION buffers.");

                            _img = ::clCreateImage2D(context(),
                                                     ((_host_ptr == nullptr ? CL_MEM_ALLOC_HOST_PTR : CL_MEM_USE_HOST_PTR) |
                                                      (read_only ? CL_MEM_READ_ONLY : CL_MEM_READ_WRITE)),
                                                     &format,
                                                     is._width,
                                                     is._height,
                                                     0,
                                                     host_ptr,
                                                     &error);
                        }
                        else
                        {
#ifdef HETCOMPUTE_HAVE_QTI_DSP
                            /// create a cl_mem_ion_host_ptr object that is passed to cl::Buffer
                            cl_mem_ion_host_ptr ionmem_host_ptr;
                            ionmem_host_ptr.ext_host_ptr.allocation_type = CL_MEM_ION_HOST_PTR_QCOM;
                            // CL_MEM_HOST_WRITEBACK_QCOM for cachable ion mem, or CL_MEM_HOST_UNCACHED_QCOM for uncachable ion mem
                            ionmem_host_ptr.ext_host_ptr.host_cache_policy =
                                is_cached ? CL_MEM_HOST_WRITEBACK_QCOM : CL_MEM_HOST_UNCACHED_QCOM;
                            ionmem_host_ptr.ion_hostptr = host_ptr;
                            HETCOMPUTE_INTERNAL_ASSERT(fd >= 0, "Error. File descritor is incorrect");
                            ionmem_host_ptr.ion_filedesc = fd;

                            if (image_cl_desc::extended_format)
                            {
                                cl_image_desc desc;

                                desc.image_type        = CL_MEM_OBJECT_IMAGE2D;
                                desc.image_width       = is._width;
                                desc.image_height      = is._height;
                                desc.image_row_pitch   = 0; // must be always set to 0 for compressed images
                                desc.image_slice_pitch = 0; // must be always set to 0 for compressed images
                                desc.num_mip_levels    = 0;
                                desc.num_samples       = 0;
                                desc.mem_object        = 0;

                                _img = ::clCreateImage(context(),
                                                       CL_MEM_USE_HOST_PTR | CL_MEM_EXT_HOST_PTR_QCOM |
                                                           (read_only ? CL_MEM_READ_ONLY : CL_MEM_READ_WRITE),
                                                       &format,
                                                       &desc,
                                                       &ionmem_host_ptr,
                                                       &error);
                            }
                            else 
                            {
                                // get row pitch to pass to clCreateImage2D
                                size_t      image_row_pitch;
                                cl::Device& device = ::hetcompute::internal::access_cldevice::get_cl_device(_device_ptr);
                                clGetDeviceImageInfoQCOM(device(), // device_id
                                                         is._width,
                                                         is._height,
                                                         &format,
                                                         CL_IMAGE_ROW_PITCH,
                                                         sizeof(image_row_pitch),
                                                         &image_row_pitch,
                                                         nullptr);
                                HETCOMPUTE_DLOG("getdeviceinfo_qcom done. image row pitch = %zu", image_row_pitch);

                                _img = ::clCreateImage2D(context(),
                                                         CL_MEM_USE_HOST_PTR | CL_MEM_EXT_HOST_PTR_QCOM | CL_MEM_READ_WRITE,
                                                         &format,
                                                         is._width,
                                                         is._height,
                                                         // must use queried row pitch. setting to 0 throws CL_INVALID_VALUE
                                                         image_row_pitch,
                                                         &ionmem_host_ptr,
                                                         &error);
                            }
#else  // HETCOMPUTE_HAVE_QTI_DSP
                            HETCOMPUTE_UNUSED(fd);
                            HETCOMPUTE_UNUSED(is_cached);
                            HETCOMPUTE_FATAL("ion texture is not supported in non-hexagon platform!");
#endif // HETCOMPUTE_HAVE_QTI_DSP
                        }
                        HETCOMPUTE_GCC_IGNORE_END("-Wdeprecated-declarations");

                        if (error != CL_SUCCESS)
                        {
                            HETCOMPUTE_ELOG("texture_cl(): clerror=%d\n", error);
                            cl::detail::errHandler(error, "texture creation failed");
                        }
#else  // HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE
                        _img        = cl::Image2D(::hetcompute::internal::access_cldevice::get_cl_context(_device_ptr),
                                           ((_host_ptr == nullptr ? CL_MEM_ALLOC_HOST_PTR : CL_MEM_USE_HOST_PTR) |
                                            (read_only ? CL_MEM_READ_ONLY : CL_MEM_READ_WRITE)),
                                           cl::img_format(image_cl_desc::order, image_cl_desc::type),
                                           is._width,
                                           is._height,
                                           size_t(0),
                                           _host_ptr,
                                           nullptr);
#endif // HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                    }
                    catch (cl::Error& err)
                    {
                        HETCOMPUTE_FATAL("Could not construct cl::Image2D in texture_cl()->%s for format: %d",
                                         ::hetcompute::internal::get_cl_error_string(err.err()),
                                         static_cast<int>(img_format));
                    }
#endif
                }

                texture_cl(image_size<2> const& is, void* host_ptr)
                    : base_texture_cl(host_ptr, { is._width, is._height, 1 }), _image_size(is)
                {
                }

                texture_cl<img_format, 2>* create_derivative_texture(extended_format_plane_type derivative_plane_type, bool read_only = false)
                {
                    texture_cl<img_format, 2>* der_texture = nullptr;

#ifdef HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE

                    if (!image_cl_desc::extended_format)
                    {
                        HETCOMPUTE_FATAL("Derivative textures are supported only for extended format.");
                    }

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                    try
                    {
#endif
                        cl_int       error;
                        cl::Context& context = ::hetcompute::internal::access_cldevice::get_cl_context(_device_ptr);

                        der_texture = new texture_cl<img_format, 2>(_image_size, nullptr);
                        if (!der_texture)
                        {
                            HETCOMPUTE_FATAL("Failed to create hetcompute derivative texture object");
                        }

                        cl_image_format der_format;
                        cl_image_desc   der_desc;
                        std::memset(&der_desc, 0, sizeof(der_desc));

                        if (derivative_plane_type == hetcompute::graphics::extended_format_plane_type::ExtendedFormatYPlane)
                        {
                            der_format.image_channel_order = image_cl_desc::order_y;
                        }
                        else
                        {
                            der_format.image_channel_order = image_cl_desc::order_uv;
                        }
                        der_format.image_channel_data_type = image_cl_desc::type;

                        der_desc.image_type   = CL_MEM_OBJECT_IMAGE2D;
                        der_desc.image_width  = _image_size._width;
                        der_desc.image_height = _image_size._height;
                        der_desc.mem_object   = _img;

                        der_texture->_img =
                            clCreateImage(context(), (read_only ? CL_MEM_READ_ONLY : CL_MEM_READ_WRITE), &der_format, &der_desc, NULL, &error);

                        if (error != CL_SUCCESS)
                        {
                            HETCOMPUTE_ELOG("create_derivative_texture(): clerror=%d\n", error);
                            cl::detail::errHandler(error, "derivative texture creation failed");
                        }
                        else
                        {
                            HETCOMPUTE_DLOG("create_derivative_texture() success for format: %d, type: 0x%x, %p",
                                          static_cast<int>(img_format),
                                          der_format.image_channel_order,
                                          der_texture);
                        }
#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                    }
                    catch (cl::Error& err)
                    {
                        HETCOMPUTE_FATAL("Could not construct cl::Image2D in texture_cl()->%s for format: %d",
                                         ::hetcompute::internal::get_cl_error_string(err.err()),
                                         static_cast<int>(img_format));
                    }
#endif

#endif

                    return der_texture;
                }

                ~texture_cl() {}
            }; // struct texture_cl<img_format, 2>

            // hetcompute 3d texture wrapper
            template <image_format img_format>
            struct texture_cl<img_format, 3> : public base_texture_cl, ::hetcompute::internal::ref_counted_object<texture_cl<img_format, 3>>
            {
            private:
                image_size<3> _image_size;
                using image_cl_desc = typename translate_image_format_to_struct<img_format>::type;

            public:
                texture_cl(image_size<3> const& is, bool read_only, void* host_ptr, int fd = -1, bool is_ion = false, bool is_cached = false)
                    : base_texture_cl(host_ptr, is), _image_size(is)
                {
                    HETCOMPUTE_API_ASSERT(host_ptr != nullptr, "Currently host_ptr == nullptr not supported. Please pre-allocate.");
                    _host_ptr = host_ptr;

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                    try
                    {
#endif

#ifdef HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE
                        cl_image_format format;
                        format.image_channel_order     = image_cl_desc::order;
                        format.image_channel_data_type = image_cl_desc::type;

                        cl_int error;

                        cl::Context& context = ::hetcompute::internal::access_cldevice::get_cl_context(_device_ptr);

                        HETCOMPUTE_GCC_IGNORE_BEGIN("-Wdeprecated-declarations");
                        HETCOMPUTE_UNUSED(fd);
                        HETCOMPUTE_UNUSED(is_cached);
                        if (!is_ion)
                        {
                            if (image_cl_desc::extended_format) HETCOMPUTE_FATAL("Extended texutre formats are only supported with ION buffers.");
                            
                            _img = ::clCreateImage3D(context(),
                                                     ((_host_ptr == nullptr ? CL_MEM_ALLOC_HOST_PTR : CL_MEM_USE_HOST_PTR) |
                                                      (read_only ? CL_MEM_READ_ONLY : CL_MEM_READ_WRITE)),
                                                     &format,
                                                     is._width,
                                                     is._height,
                                                     is._depth,
                                                     0, // auto-calculate row_pitch
                                                     0, // auto-calculate slice_pitch
                                                     host_ptr,
                                                     &error);
                        }
                        else
                        {
                            HETCOMPUTE_FATAL("hetcompute doesn't support ion 3d texture yet.");
                        }
                        HETCOMPUTE_GCC_IGNORE_END("-Wdeprecated-declarations");

                        HETCOMPUTE_DLOG("texture_cl(): clerror=%d\n", error);
                        cl::detail::errHandler(error, "texture creation failed");
#else  // HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE
                        _img        = cl::Image3D(::hetcompute::internal::access_cldevice::get_cl_context(_device_ptr),
                                           ((_host_ptr == nullptr ? CL_MEM_ALLOC_HOST_PTR : CL_MEM_USE_HOST_PTR) |
                                            (read_only ? CL_MEM_READ_ONLY : CL_MEM_READ_WRITE)),
                                           cl::img_format(image_cl_desc::order, image_cl_desc::type),
                                           is._width,
                                           is._height,
                                           is._depth,
                                           0, // auto-calculate row_pitch
                                           0, // auto-calculate slice_pitch
                                           _host_ptr,
                                           nullptr);
#endif // HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                    }
                    catch (cl::Error& err)
                    {
                        HETCOMPUTE_FATAL("Could not construct cl::Image3D in texture_cl()->%s for format: %d",
                                         ::hetcompute::internal::get_cl_error_string(err.err()),
                                         static_cast<int>(img_format));
                    }
#endif
                }

                ~texture_cl() {}
            }; // struct texture_cl<img_format, 3>

            // hetcompute sampler_cl wraps cl_sampler
            template <addressing_mode addr_mode, filter_mode fil_mode>
            struct sampler_cl : public ::hetcompute::internal::ref_counted_object<sampler_cl<addr_mode, fil_mode>>
            {
                ::hetcompute::internal::legacy::device_ptr _device_ptr;

#ifdef HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE
                cl_sampler _sampler;
#endif

            public:
                explicit sampler_cl(bool normalized_coords) : _device_ptr(::hetcompute::internal::s_dev_ptrs->at(0)), _sampler()
                {

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                    try
                    {
#endif

#ifdef HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE

                        cl_int error;

                        cl::Context& context = ::hetcompute::internal::access_cldevice::get_cl_context(_device_ptr);

                        HETCOMPUTE_GCC_IGNORE_BEGIN("-Wdeprecated-declarations");
                        _sampler = ::clCreateSampler(context(),
                                                     normalized_coords,
                                                     translate_addressing_mode<addr_mode>::cl_val,
                                                     translate_filter_mode<fil_mode>::cl_val,
                                                     &error);
                        HETCOMPUTE_GCC_IGNORE_END("-Wdeprecated-declarations");

                        HETCOMPUTE_DLOG("sampler_cl(): clerror=%d\n", error);

                        cl::detail::errHandler(error, "sampler creation failed");

#endif

#ifndef HETCOMPUTE_DISABLE_EXCEPTIONS
                    }
                    catch (cl::Error& err)
                    {
                        HETCOMPUTE_FATAL("Could not construct cl::sampler in sampler_cl()->%s",
                                         ::hetcompute::internal::get_cl_error_string(err.err()));
                    }
#endif

                }

                ~sampler_cl()
                {
                    HETCOMPUTE_DLOG("DESTRUCTING sampler %p", _sampler);
#ifdef HETCOMPUTE_USE_OPENCL_C_APIS_FOR_TEXTURE
                    cl_int status = ::clReleaseSampler(_sampler);
                    if (status != CL_SUCCESS)
                    {
                        HETCOMPUTE_FATAL("Failed to release sampler %p", _sampler);
                    }
#endif
                }
            }; // class sampler_cl

        };  // namespace internal
    };  // namespace graphics
};  // namespace hetcompute

namespace hetcompute
{
    namespace graphics
    {
        namespace internal
        {
            // supported image formats are detected at runtime for the device
            extern std::set<cl::ImageFormat, image_format_less>* p_supported_image_format_set;
        }; // namespace internal
    };     // namespace graphics
};         // namespace hetcompute

namespace hetcompute
{
    namespace graphics
    {
        namespace internal
        {
            /// Converts image format representation:
            ///    hetcompute::graphics::image_format enum value to corresponding cl::ImageFormat value
            ///
            /// @param hetcompute_image_format A hetcompute::graphics::image_format enum value
            /// @return cl::ImageFormat value corresponding to hetcompute_image_format
            cl::ImageFormat get_cl_image_format(image_format hetcompute_image_format);

            /// Converts image format representation:
            ///    cl::ImageFormat value to corresponding hetcompute::graphics::image_format enum value
            ///
            /// @param cl_img_fmt A cl::ImageFormat struct value
            /// @return hetcompute::graphics::image_format enum value corresponding to cl_img_fmt
            image_format get_hetcompute_image_format(cl::ImageFormat cl_img_fmt);
        };
    };
};

namespace hetcompute
{
    namespace graphics
    {
        template <image_format img_format, int dims>
        using texture_ptr = ::hetcompute::internal::hetcompute_shared_ptr<internal::texture_cl<img_format, dims>>;

        template <image_format img_format, int dims, typename T>
        texture_ptr<img_format, dims> create_texture(image_size<dims> const& is, T* host_ptr)
        {
            static_assert(dims == 1 || dims == 2 || dims == 3, "texture_ptr(): currently only dims = 1, 2 or 3 is supported");
            using non_const_T = typename std::remove_const<T>::type;

            return texture_ptr<img_format, dims>(
                new internal::texture_cl<img_format, dims>(is,
                                                           std::is_const<T>::value,
                                                           reinterpret_cast<void*>(const_cast<non_const_T*>(host_ptr)),
                                                           false,
                                                           false));
        }

#ifdef HETCOMPUTE_HAVE_QTI_DSP
        // ion memregion is only supported in HEXAGON
        template <image_format img_format, int dims>
        texture_ptr<img_format, dims> create_texture(image_size<dims> const& is, ion_memregion& ion_mr, bool read_only = false)
        {
            static_assert(dims == 2 || dims == 3, "texture_ptr(): currently only dims = 2 or 3 is supported");

            // FIXME: ideally we should calculate size of image needed based on img_format
            HETCOMPUTE_API_ASSERT(ion_mr.get_num_bytes() > 0, "Memregion has zero bytes.");
            auto int_ion_mr = ::hetcompute::internal::memregion_base_accessor::get_internal_mr(ion_mr);
            HETCOMPUTE_INTERNAL_ASSERT(int_ion_mr != nullptr, "Error. Internal memregion member is null");
            HETCOMPUTE_INTERNAL_ASSERT(int_ion_mr->get_type() == ::hetcompute::internal::memregion_t::ion, "Error. Incorrect type of memregion");
            return texture_ptr<img_format, dims>(
                new internal::texture_cl<img_format, dims>(is, read_only, ion_mr.get_ptr(), ion_mr.get_fd(), true, int_ion_mr->is_cacheable()));
        }

        // Create derivative texture from parent texture. This is only for extended formats.
        template <image_format img_format, int dims>
        texture_ptr<img_format, dims> create_derivative_texture(texture_ptr<img_format, dims>& parent_texture,
                                                                extended_format_plane_type     derivative_plane_type,
                                                                bool                           read_only = false)
        {
            auto internal_tp = c_ptr(parent_texture);
            HETCOMPUTE_INTERNAL_ASSERT(internal_tp != nullptr, "hetcompute_shared_ptr of texture_cl is nullptr");
            static_assert(dims == 2, "texture_ptr(): currently only dims = 2 is supported");
            return texture_ptr<img_format, dims>(internal_tp->create_derivative_texture(derivative_plane_type, read_only));
        }
#endif

        template <image_format img_format, int dims>
        void* map(texture_ptr<img_format, dims>& tp)
        {
            auto internal_tp = c_ptr(tp);
            HETCOMPUTE_INTERNAL_ASSERT(internal_tp != nullptr, "hetcompute_shared_ptr of texture_cl is nullptr");
            return internal_tp->map();
        }

        template <image_format img_format, int dims>
        void* map(texture_ptr<img_format, dims>& tp, image_size<3>& image_dims)
        {
            auto internal_tp = c_ptr(tp);
            HETCOMPUTE_INTERNAL_ASSERT(internal_tp != nullptr, "hetcompute_shared_ptr of texture_cl is nullptr");
            return internal_tp->map(image_dims);
        }

        template <image_format img_format, int dims>
        void unmap(texture_ptr<img_format, dims>& tp)
        {
            auto internal_tp = c_ptr(tp);
            HETCOMPUTE_INTERNAL_ASSERT(internal_tp != nullptr, "hetcompute_shared_ptr of texture_cl is nullptr");
            internal_tp->unmap();
        }

        // sampler
        template <addressing_mode addr_mode, filter_mode fil_mode>
        using sampler_ptr = ::hetcompute::internal::hetcompute_shared_ptr<internal::sampler_cl<addr_mode, fil_mode>>;

        template <addressing_mode addr_mode, filter_mode fil_mode>
        sampler_ptr<addr_mode, fil_mode> create_sampler(bool normalized_coords)
        {
            return sampler_ptr<addr_mode, fil_mode>(new internal::sampler_cl<addr_mode, fil_mode>(normalized_coords));
        }

        template <image_format img_format>
        bool is_supported()
        {
            using image_cl_desc = typename hetcompute::graphics::internal::translate_image_format_to_struct<img_format>::type;
            cl::ImageFormat format(image_cl_desc::order, image_cl_desc::type);
            return internal::p_supported_image_format_set->count(format) > 0;
        }

        bool is_supported(image_format img_format);
    };  // namespace graphics
};  // namespace hetcompute

namespace hetcompute
{
    namespace graphics
    {
        namespace internal
        {
            template <typename TexturePtr>
            struct texture_ptr_info;

            template <image_format ImgFormat, int Dims>
            struct texture_ptr_info<texture_ptr<ImgFormat, Dims>>
            {
                static constexpr image_format img_format = ImgFormat;
                static constexpr int          dims       = Dims;
            };

            template <int dims_to, int dims_from>
            struct image_size_converter;

            template <>
            struct image_size_converter<1, 1>
            {
                static image_size<1> convert(image_size<1> const& is) { return is; }
            };

            template <>
            struct image_size_converter<1, 2>
            {
                static image_size<1> convert(image_size<2> const& is) { return image_size<1>{ is._width }; }
            };

            template <>
            struct image_size_converter<1, 3>
            {
                static image_size<1> convert(image_size<3> const& is) { return image_size<1>{ is._width }; }
            };

            template <>
            struct image_size_converter<2, 1>
            {
                static image_size<2> convert(image_size<1> const& is) { return image_size<2>{ is._width, 1 }; }
            };

            template <>
            struct image_size_converter<2, 2>
            {
                static image_size<2> convert(image_size<2> const& is) { return is; }
            };

            template <>
            struct image_size_converter<2, 3>
            {
                static image_size<2> convert(image_size<3> const& is) { return image_size<2>{ is._width, is._height }; }
            };

            template <>
            struct image_size_converter<3, 1>
            {
                static image_size<3> convert(image_size<1> const& is) { return image_size<3>{ is._width, 1, 1 }; }
            };

            template <>
            struct image_size_converter<3, 2>
            {
                static image_size<3> convert(image_size<2> const& is) { return image_size<3>{ is._width, is._height, 1 }; }
            };

            template <>
            struct image_size_converter<3, 3>
            {
                static image_size<3> convert(image_size<3> const& is) { return is; }
            };

        }; // namespace internal
    };     // namespace graphics
};         // namespace hetcompute

#endif // HETCOMPUTE_HAVE_OPENCL
