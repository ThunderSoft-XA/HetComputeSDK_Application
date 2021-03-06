#include <cmath>
#include <fstream>
#include <string.h>
#include <sys/time.h>
#include <hetcompute/hetcompute.hh>
// header to include the dsp bindings, it is generated by the Hexagon SDK
#include <include/hetcompute_dsp.h>
#include <hetcompute/gpukernel.hh>

#ifdef __ANDROID__
#define DEFAULT_INPUT_FILE "/mnt/sdcard/SampleGrayImageUncompressed_dep.tga"
#else
#define DEFAULT_INPUT_FILE "SampleGrayImageUncompressed_dep.tga"
#endif // __ANDROID__

#ifdef __ANDROID__
    char const* output_filename = "/mnt/sdcard/output_image_task_dep.tga";
#else
    char const* output_filename = "output_image_task_dep.tga";
#endif

#define Pixel unsigned char

#define MAX_DIST (255 * 255)
#define SEARCH_WINDOW_SIZE 21
#define SIMILARITY_WINDOW_SIZE 7

struct Point
{
    int x; // the col, width;
    int y; // the row, height
};

// Describes the header structure of uncompressed TGA images.
struct tgaheadertype
{
    unsigned char  _id_length, _color_map_type, _image_type;
    unsigned short _cmap_start, _cmap_length;
    unsigned char  _cmap_depth;
    unsigned short _xOffset, _yOffset, _width, _height;
    unsigned char  _pixel_depth, _image_descriptor;
} __attribute__((packed));

static unsigned char pixel_depth;
static unsigned char image_type;
static unsigned int  img_width;
static unsigned int  img_height;
static unsigned int  input_buffer_size;
static unsigned int  output_buffer_size;

using namespace hetcompute;

long getCurrentTimeMsec();
void read_tga(char const* file_name, Pixel*& src);
void write_tga(char const* file_name, Pixel*& data);


void move_input_data(Pixel const *in, hetcompute::buffer_ptr<unsigned char> out)
{
    for (size_t x = 0; x < out.size(); x++) {
        out[x] = in[x];
    }
}

void move_output_data(hetcompute::buffer_ptr<const float> in, Pixel *out)
{
    for (size_t x = 0; x < in.size(); x++) {
        out[x] = in[x];
    }
}


// Create a string containing OpenCL C kernel code.
#define OCL_KERNEL(name, k) std::string const name##_string = #k

OCL_KERNEL(image_kernel, 
           __kernel void process_denoise_image(__global unsigned char* input_buffer, 
                                               __global float* output_buffer, 
                                               __global float* similarity_weights_buffer, 
                                               unsigned int searchWindowSize, 
                                               unsigned int similarityWindowSize, 
                                               unsigned int imgWidth, 
                                               unsigned int imgHeight) {
    unsigned int height = get_global_id(1);
    unsigned int width = get_global_id(0);

    float reusable_buffer[441];

    for (int i = 0; i < searchWindowSize; i++) {
        for (int j = 0; j < searchWindowSize; j++) {
            int2 point = (int2) (width, height);
            int2 neighbor = (int2) (0, 0);

            neighbor.x = point.x - searchWindowSize / 2 + i;
            neighbor.y = point.y - searchWindowSize / 2 + j;

            neighbor.x = abs(neighbor.x);
            neighbor.y = abs(neighbor.y);

            if (neighbor.x >= imgWidth) {
                neighbor.x = imgWidth - (neighbor.x - imgWidth) - 1;
            }

            if (neighbor.y >= imgHeight) {
                neighbor.y = imgHeight - (neighbor.y - imgHeight) - 1;
            }

            int area = similarityWindowSize * similarityWindowSize;
            int color_dist_sum = 0;
            int2 p_neighbor = (int2) (0, 0);
            int2 ref_neighbor = (int2) (0, 0);

            for (int ii = 0; ii < similarityWindowSize; ii++) {
                for (int jj = 0; jj < similarityWindowSize; jj++) {
                    p_neighbor.x = neighbor.x - similarityWindowSize / 2 + jj;
                    p_neighbor.y = neighbor.y - similarityWindowSize / 2 + ii;

                    p_neighbor.x = abs(p_neighbor.x);
                    p_neighbor.y = abs(p_neighbor.y);

                    if (p_neighbor.x >= imgWidth) {
                        p_neighbor.x = imgWidth - (p_neighbor.x - imgWidth) - 1;
                    }

                    if (p_neighbor.y >= imgHeight) {
                        p_neighbor.y = imgHeight - (p_neighbor.y - imgHeight) - 1;
                    }


                    ref_neighbor.x = point.x - similarityWindowSize / 2 + jj;
                    ref_neighbor.y = point.y - similarityWindowSize / 2 + ii;

                    ref_neighbor.x = abs(ref_neighbor.x);
                    ref_neighbor.y = abs(ref_neighbor.y);

                    if (ref_neighbor.x >= imgWidth) {
                        ref_neighbor.x = imgWidth - (ref_neighbor.x - imgWidth) - 1;
                    }

                    if (ref_neighbor.y >= imgHeight) {
                        ref_neighbor.y = imgHeight - (ref_neighbor.y - imgHeight) - 1;
                    }

                    int dist = input_buffer[p_neighbor.y * imgWidth + p_neighbor.x] - input_buffer[ref_neighbor.y * imgWidth + ref_neighbor.x];
                    color_dist_sum += dist * dist;
                }
            }

            color_dist_sum /= area;
            reusable_buffer[i * searchWindowSize + j] = similarity_weights_buffer[color_dist_sum];
        }
    }

    float weight_sum = 0;
    float temp = 0;

    for (int m = 0; m < searchWindowSize; m++) {
        for (int n = 0; n < searchWindowSize; n++) {
            int2 search_neighbor = (int2) (0, 0);
            search_neighbor.x = width - searchWindowSize / 2 + m;
            search_neighbor.y = height - searchWindowSize / 2 + n;

            search_neighbor.x = abs(search_neighbor.x);
            search_neighbor.y = abs(search_neighbor.y);

            if (search_neighbor.x >= imgWidth) {
                search_neighbor.x = imgWidth - (search_neighbor.x - imgWidth) - 1;
            }

            if (search_neighbor.y >= imgHeight) {
                search_neighbor.y = imgHeight - (search_neighbor.y - imgHeight) - 1;
            }

            temp += reusable_buffer[m * searchWindowSize + n] * input_buffer[search_neighbor.y * imgWidth + search_neighbor.x];
            weight_sum += reusable_buffer[m * searchWindowSize + n];
        }
    }

    temp /= weight_sum;
    output_buffer[height * imgWidth + width] = temp;
});


int
main(int argc, char *argv[])
{
    hetcompute::runtime::init();

    // This is to ensure all buffers are deleted before we call shutdown.
    {
        const char* inputfile;

        if (argc != 1) {
            HETCOMPUTE_ILOG("******************************************************************************************");
            HETCOMPUTE_ILOG("Application don't need to input parameter.");
            HETCOMPUTE_ILOG("eg: ./hetcompute_sample_ParallelTaskDependencyDemo");
            HETCOMPUTE_ILOG("******************************************************************************************");

            return -1;
        } else {
            inputfile = DEFAULT_INPUT_FILE;                          //ifdef __ANDROID__
        }

        // Create system time save
        unsigned long begin_process_time = 0;
        unsigned long end_process_time = 0;

        // Create image date buffer ptr
        Pixel* input_img_data = nullptr;
        Pixel* output_img_data = nullptr;

        // Read image data
        read_tga(inputfile, input_img_data);　　　　　　　　　　 //加载照片　　　　　　　　　　　　　　　　
        output_img_data = new Pixel[output_buffer_size];　　　

        // Create HetComputeSDK buffer
        auto input_buffer = hetcompute::create_buffer<unsigned char>(input_buffer_size, hetcompute::device_set({ hetcompute::cpu, hetcompute::gpu }));
        auto output_buffer = hetcompute::create_buffer<float>(output_buffer_size, hetcompute::device_set({ hetcompute::gpu }));
        auto similarity_weights_buffer = hetcompute::create_buffer<float>(MAX_DIST, hetcompute::device_set({ hetcompute::dsp, hetcompute::gpu }));　　　//!===@!
        //cpu  gpu dsp 数据交互

        // Create CPU task, copy image date to ION buffer.　　
        auto ct1 = hetcompute::create_task(move_input_data, input_img_data, input_buffer);

        // Create DSP kernel channel　                                                           table在dsp代码中实现.so库放到het_computer原始工程中
        auto dsp_kernel = hetcompute::create_dsp_kernel<>(hetcompute_dsp_compute_intensity_dist_weight_table);
        // Create DSP task　　　　　　　　
        auto dt = hetcompute::create_task(dsp_kernel, similarity_weights_buffer);

        // Create GPU kernel channel　　                                                          gpu的运算任务
        auto gk = hetcompute::create_gpu_kernel<hetcompute::buffer_ptr<const unsigned char>, 
                                                hetcompute::buffer_ptr<float>, 
                                                hetcompute::buffer_ptr<const float>, 
                                                const unsigned int, 
                                                const unsigned int, 
                                                const unsigned int, 
                                                const unsigned int>
                                                (image_kernel_string, "process_denoise_image");
        // [Create a 2D Range Task]
        hetcompute::range<2> range_2d(img_width, img_height);
        // Create a task
        auto gt = hetcompute::create_task(gk, range_2d, input_buffer, output_buffer, 
                                          similarity_weights_buffer, 
                                          SEARCH_WINDOW_SIZE, SIMILARITY_WINDOW_SIZE, 
                                          img_width, img_height);

        // Create CPU task, copy ION buffer date to make image　　将处理好的数据转化成照片
        auto ct2 = hetcompute::create_task(move_output_data, output_buffer, output_img_data);

        // Create a heterogeneous task DAG consisting of control and data
        // task dependencies
        dt->then(ct1)->then(gt)->then(ct2);
        // launch all tasks for execution　　                     dsp把wighet table做好　gpu算法去除噪点
        ct1->launch();
        dt->launch();
        gt->launch();
        ct2->launch();
        // Wait for the DAG to finish
        begin_process_time = getCurrentTimeMsec();
        ct2->wait_for();　　　　　　　　　　　　　　　　                //het_compute让任务跑起来
        end_process_time = getCurrentTimeMsec();

        // Write data image
        write_tga(output_filename, output_img_data);

        // Delete all memory object
        delete [] input_img_data;
        delete [] output_img_data;

        HETCOMPUTE_ILOG("******Running all tasks total time is: %ld ms", end_process_time - begin_process_time);
    }

    
    hetcompute::runtime::shutdown();
    return 0;
}

long
getCurrentTimeMsec()
{
    long msec = 0;
    char str[20] = {0};
    struct timeval stuCurrentTime;

    gettimeofday(&stuCurrentTime, NULL);
    sprintf(str, "%ld%03ld", stuCurrentTime.tv_sec, (stuCurrentTime.tv_usec) / 1000);

    for (int i = 0; i < strlen(str); i++) {
        msec = msec * 10 + (str[i] - '0');
    }

    return msec;
}

void
read_tga(char const* file_name, Pixel*& src)
{
    std::ifstream tga(file_name, std::ios::binary);
    if (!tga.is_open())
    {
        HETCOMPUTE_FATAL("Could not open src image file: %s", file_name);
    }
    tgaheadertype tga_header;
    tga.read(reinterpret_cast<char*>(&tga_header), sizeof(tga_header));
    img_width  = tga_header._width;
    img_height = tga_header._height;

    pixel_depth = tga_header._pixel_depth;
    image_type  = tga_header._image_type;
    HETCOMPUTE_ILOG("image format: %d, pixel depth: %d, width: %d, height: %d\n",
                    int(tga_header._image_type),
                    int(tga_header._pixel_depth),
                    int(img_width),
                    int(img_height));

    if (pixel_depth != 8)
        HETCOMPUTE_FATAL("PROCESSING GRAY IMAGE WITH DEPTH OF 1 BYTE ONLY\n");

    int bytes_per_pixel = int(tga_header._pixel_depth) / 8;
    input_buffer_size = img_width * img_height * bytes_per_pixel * sizeof(Pixel);
    output_buffer_size = img_width * img_height * sizeof(Pixel);
    src = new Pixel[input_buffer_size];
    tga.read(reinterpret_cast<char*>(src), img_width * img_height * bytes_per_pixel * sizeof(Pixel));
}

void
write_tga(char const* file_name, Pixel*& data)
{
    tgaheadertype tga_header;
    HETCOMPUTE_ILOG("tga header size: %d\n", int(sizeof(tga_header)));
    memset(&tga_header, 0, sizeof(tga_header));
    tga_header._image_type  = image_type;
    tga_header._width       = img_width;
    tga_header._height      = img_height;
    tga_header._pixel_depth = pixel_depth; // grey image 8

    std::ofstream tga(file_name, std::ios::binary);
    if (!tga.is_open())
    {
        HETCOMPUTE_FATAL("Could not open output image file: %s", file_name);
    }

    tga.write(reinterpret_cast<char*>(&tga_header), sizeof(tga_header));
    tga.write(reinterpret_cast<char*>(data), img_width * img_height * sizeof(Pixel));               //het_compute
}