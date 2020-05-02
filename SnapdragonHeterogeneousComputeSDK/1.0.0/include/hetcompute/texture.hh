/** @file texture.hh */
#pragma once

#ifdef HETCOMPUTE_HAVE_OPENCL

// User-visible headers first
#include <hetcompute/texturetype.hh>

// Include internal headers next
#include <hetcompute/internal/graphics/texture.hh>

namespace hetcompute
{
    namespace graphics
    {
        /** @addtogroup texture_doc
           @{ */

        template <image_format img_format, int dims>
        using texture_ptr = ::hetcompute::internal::hetcompute_shared_ptr<internal::texture_cl<img_format, dims>>;

        /**
            @brief Create Qualcomm HetCompute texture with create_texture(...).
            Create HetCompute texture with create_texture(...)
            @tparam img_format  Qualcomm HetCompute image format.
            @tparam dims        Image dimensions.
            @param is           Image dimension size.
            @param host_ptr     Host pointer to image data in CPU. host_ptr must not be nullptr.
            @return             A pointer to the created texture.
        */
        template <image_format img_format, int dims, typename T>
        texture_ptr<img_format, dims> create_texture(image_size<dims> const& is, T* host_ptr);

#ifdef HETCOMPUTE_HAVE_QTI_DSP
        /**
            @brief Create HetCompute texture with create_texture(...) using ion memory
            Create HetCompute texture with create_texture(...) using ion memory
            @tparam img_format  HetCompute image format
            @tparam dims       image dimensions
            @param is          image dimension size
            @param ion_mr      instance of ion_memregion
            @return            a pointer to the created texture
        */
        template <image_format img_format, int dims>
        texture_ptr<img_format, dims> create_texture(image_size<dims> const& is, ion_memregion const& ion_mr, bool read_only = false);

        /**
            @brief Create HetCompute single-plane derivative texture from a multi-plane parent texture.
            The parent texture is created with create_texture(...) using ION memory

            The following OpenCL QCOM extension are adopted for this feature
    
            Extract Derivative Image Plane: cl_qcom_extract_image_plane
            QCOM Supported Compressed Image: cl_qcom_compressed_image
            QCOM Other Non-Conventional Images [NV12, TP10]: cl_qcom_other_image

            Create HetCompute single-plane derivative texture from a multi-plane parent HetCompute texture.
            The parent texture is created with create_texture(...) using ION memory
            @tparam img_format           HetCompute image format
            @tparam dims                 image dimensions
            @param parent_texture        multi-plane parent texture created using create_texture(...)
            @param derivative_plane_type Type of child plane (Y or UV)
            @param read_only             indicates if the created derivative texture should be RO/RW
            @return                      a pointer to the created derivative texture
            @note                        img_format should match the image format of parent texture
        */
        template <image_format img_format, int dims>
        texture_ptr<img_format, dims> create_derivative_texture(texture_ptr<img_format, dims>& parent_texture,
                                                                extended_format_plane_type     derivative_plane_type,
                                                                bool                           read_only);

#endif // HETCOMPUTE_HAVE_QTI_DSP

        /**
            @brief Map data from GPU to CPU with map(...).
            Map data from GPU to CPU with map(...).
            @tparam img_format Qualcomm HetCompute image format.
            @tparam dims       Image dimensions.
            @param tp          Qualcomm HetCompute texture pointer.
            @return            A pointer to image data in CPU.
        */
        template <image_format img_format, int dims>
        void* map(texture_ptr<img_format, dims>& tp);

        /**
            @brief Unmap data from CPU to GPU with unmap(...).
            Unmap data from CPU to GPU with unmap(...).
            @tparam img_format Qualcomm HetCompute image format.
            @tparam dims       Image dimensions.
            @param tp          Qualcomm HetCompute texture pointer.
        */
        template <image_format img_format, int dims>
        void unmap(texture_ptr<img_format, dims>& tp);

        template <addressing_mode addr_mode, filter_mode fil_mode>
        using sampler_ptr = ::hetcompute::internal::hetcompute_shared_ptr<internal::sampler_cl<addr_mode, fil_mode>>;

        /**
            @brief Create HetCompute sampler with create_sampler(...).
            Create HetCompute sampler with create_sampler(...).
            @tparam addr_mode         Addressing mode.
            @tparam fil_mode          Filtering mode.
            @param normalized_coords  Whether to use normalized coordinates for pixel access.
            @return                   A pointer to the created sampler.
        */
        template <addressing_mode addr_mode, filter_mode fil_mode>
        sampler_ptr<addr_mode, fil_mode> create_sampler(bool normalized_coords);

        /**
            @brief Test if given image format is supported by current platform
            and context at runtime.
            @param img_format Qualcomm HetCompute image format.
            @return Ff this image format is supported by current device.
        */
        bool is_supported(image_format img_format);

        /** @} */ /* end_addtogroup texture_doc */

    }; // namespace graphics
};     // namespace hetcompute

#endif // HETCOMPUTE_HAVE_OPENCL
