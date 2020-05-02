#pragma once

#ifdef HETCOMPUTE_HAVE_OPENCL

namespace hetcompute
{
    namespace graphics
    {
        namespace internal
        {
            void setup_supported_image_format_set();
            void delete_supported_image_format_set();

            void setup_image_format_conversions();
            void cleanup_image_format_conversions();

            struct image_format_less
            {
                bool operator() (cl::ImageFormat const & img_format1, cl::ImageFormat const & img_format2) const
                {
                    return (img_format1.image_channel_order < img_format2.image_channel_order ||
                            ((img_format1.image_channel_order == img_format2.image_channel_order) &&
                             (img_format1.image_channel_data_type < img_format2.image_channel_data_type)));
                }
            };  // struct image_format_less
        };
    };
};

#endif // HETCOMPUTE_HAVE_OPENCL
