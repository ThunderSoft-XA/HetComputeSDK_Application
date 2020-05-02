/** @file texturetype.hh */
#pragma once

#ifdef HETCOMPUTE_HAVE_OPENCL

#include <hetcompute/internal/compat/compiler_compat.h>

namespace hetcompute
{
    namespace graphics
    {
        /** @addtogroup texturetype_doc
           @{ */

        /**
            Supported image format in Qualcomm HetCompute.
            Each format can be mapped to OpenCL image format and pixel channel.
        */
        enum class image_format : int
        {
            first,

            // 4 color channel formats
            RGBAsnorm_int8 = first, // 0
            RGBAunorm_int8,
            RGBAsigned_int8,
            RGBAunsigned_int8,
            RGBAunorm_int16,
            RGBA_float,
            RGBA_half,

            ARGBsnorm_int8, // 7
            ARGBunorm_int8,
            ARGBsigned_int8,
            ARGBunsigned_int8,

            BGRAsnorm_int8, // 11
            BGRAunorm_int8,
            BGRAsigned_int8,
            BGRAunsigned_int8,

            // 2 color channel formats
            RGsnorm_int8, // 15
            RGunorm_int8,
            RGsigned_int8,
            RGunsigned_int8,
            RGunorm_int16,
            RG_float,
            RG_half,

            // 1 color channel formats
            INTENSITYsnorm_int8, // 22
            INTENSITYsnorm_int16,
            INTENSITYunorm_int8,
            INTENSITYunorm_int16,
            INTENSITY_float,

            LUMINANCEsnorm_int8, // 27
            LUMINANCEsnorm_int16,
            LUMINANCEunorm_int8,
            LUMINANCEunorm_int16,
            LUMINANCE_float,

            Rsnorm_int8, // 32
            Runorm_int8,
            Rsigned_int8,
            Runsigned_int8,
            Runorm_int16,
            R_float,
            R_half,

            // Qualcomm extended formats
            NV12unorm_int8, // 39
            P010unorm_int10,
            TP10unorm_int10,

            TiledNV12unorm_int8, // 42
            TiledP010unorm_int10,
            TiledTP10unorm_int10,

            CompressedNV12unorm_int8, // 45
            CompressedNV124Runorm_int8,
            CompressedP010unorm_int10,
            CompressedTP10unorm_int10,

            last = CompressedTP10unorm_int10
        };

        /**
            Supported image filter mode in Qualcomm HetCompute.
            Each mode can be mapped to OpenCL sampler filter mode.
        */
        enum class filter_mode
        {
            FILTER_NEAREST,
            FILTER_LINEAR
        };

        /**
            Supported image addressing mode in Qualcomm HetCompute.
            Each mode can be mapped to OpenCL sampler addressing mode.
        */
        enum class addressing_mode
        {
            ADDRESS_NONE,
            ADDRESS_CLAMP_TO_EDGE,
            ADDRESS_CLAMP,
            ADDRESS_REPEAT
        };

        /** @} */ /* end_addtogroup texturetype_doc */

        /** @addtogroup texturetype_doc
           @{ */

        /**
            HetCompute image dimension description.
        */
        template <int dims>
        struct image_size
        {
        };

        /**
            Qualcomm HetCompute 1D image dimension description.
        */
        template <>
        struct image_size<1>
        {
            size_t _width;
        };

        /**
            Qualcomm HetCompute 2D image dimension description.
        */
        template <>
        struct image_size<2>
        {
            size_t _width;  /**< Width. */
            size_t _height; /**< Height. */
        };

        /**
            Qualcomm HetCompute 3D image dimension description.
        */
        template <>
        struct image_size<3>
        {
            size_t _width;  /**< Width. */
            size_t _height; /**< Height. */
            size_t _depth;  /**< Depth. */
        };

        /**
            Qualcomm HetCompute supported extended format derivative types
        */
        enum class extended_format_plane_type
        {
            ExtendedFormatYPlane,
            ExtendedFormatUVPlane
        };

        /** @} */ /* end_addtogroup texturetype_doc */

    }; // namespace graphics
};     // namespace hetcompute

#endif // HETCOMPUTE_HAVE_OPENCL
