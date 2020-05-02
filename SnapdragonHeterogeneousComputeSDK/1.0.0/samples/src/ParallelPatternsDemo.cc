#include <random>
#include <string.h>
#include <sys/time.h>
#include <hetcompute/hetcompute.hh>

#define RANDOM_MAX_VALUE 20000
#define VEC_MATRIX_SIZE 10
#define LOOP_NUM 100

static float process_calc_time = 0;


void matrix_parallel_scan(std::vector<int> inputData);
void matrix_iteration_process(std::vector<int> DataA, std::vector<int> DataB);
void matrix_preduce_process(std::vector<int> inputData);


long getCurrentTimeMsec()
{
    unsigned long msec = 0;
    char str[20] = {0};
    struct timeval stuCurrentTime;

    gettimeofday(&stuCurrentTime, NULL);
    sprintf(str, "%ld%03ld", stuCurrentTime.tv_sec, (stuCurrentTime.tv_usec) / 1000);

    for (int i = 0; i < strlen(str); i++) {
        msec = msec * 10 + (str[i] - '0');
    }

    return msec;
}


int
main(int argc, char *argv[])
{
    hetcompute::runtime::init();

    // This is to ensure all buffers are deleted before we call shutdown.
    if (argc > 2) {
        HETCOMPUTE_ILOG("********************************************");
        HETCOMPUTE_ILOG("eg: ./hetcompute_sample_ParallelPatternsDemo");
        HETCOMPUTE_ILOG("********************************************");

        return -1;
    }

    // Create vector martix
    std::vector<int> matrixDataA(VEC_MATRIX_SIZE, 1); //Init matrix all parameters to 1
    std::vector<int> matrixDataB(VEC_MATRIX_SIZE, 0);

    // initialize the input array with random numbers
    /*std::random_device              random_device;
    std::mt19937                    generator(random_device());
    const int                       min_value = 0, max_value = RANDOM_MAX_VALUE;
    std::uniform_int_distribution<> dist(min_value, max_value);
    auto                            gen_dist = std::bind(dist, std::ref(generator));

    std::generate(matrixDataA.begin(), matrixDataA.end(), gen_dist);*/

    // CPU - pscan_inclusive init the matrix
    // matrixDataA[x] = matrixDataA[x - 1] + matrixDataA[x]
    matrix_parallel_scan(matrixDataA);                     //向量数组初始化

    // CPU - Iteration program
    // By Iteration function copy matrix A data to B
    matrix_iteration_process(matrixDataA, matrixDataB);　　//向量数组迭代

    // CPU - preduce program
    // Summing all matrix[x] value
    matrix_preduce_process(matrixDataA);　　　　            //数组的加和

    hetcompute::runtime::shutdown();
    return 0;
}


void matrix_iteration_process(std::vector<int> DataA, std::vector<int> DataB)
{
    unsigned long begin_process_time = 0;
    unsigned long end_process_time = 0;

    int parallel_sum = 0;

    // Run parallel calc from pfor_each
    begin_process_time = getCurrentTimeMsec();

    for (size_t x = 0; x < LOOP_NUM; x++) {

        hetcompute::pfor_each(size_t(0), DataA.size(), [DataA, &DataB](size_t i) {
            DataB[i] += DataA[i] * 2;
        });
    }

    end_process_time = getCurrentTimeMsec();
    process_calc_time = (float)(end_process_time - begin_process_time);

    HETCOMPUTE_ILOG("matrix_iteration_process function copy matrix A data to B, 100 times consume time: %f ms.", process_calc_time);
}


void matrix_preduce_process(std::vector<int> inputData)
{
    unsigned long begin_process_time = 0;
    unsigned long end_process_time = 0;

    const int identity = 0;
    int parallel_sum = 0;

    // Run parallel calc from preduce
    begin_process_time = getCurrentTimeMsec();

    for (size_t x = 0; x < LOOP_NUM; x++) {
        parallel_sum = hetcompute::preduce(size_t(0), inputData.size(), identity, 
                                           [inputData](size_t f, size_t l, int& init) {
                                               for (size_t k = f; k < l; ++k) {
                                                   init += inputData[k];
                                               }
                                           }, 
                                           std::plus<int>());
    }
    end_process_time = getCurrentTimeMsec();
    process_calc_time = (float)(end_process_time - begin_process_time);

    HETCOMPUTE_ILOG("matrix_preduce_process function matrix[0] + .. + matrix[%d], %d times consume time: %f ms, sum: %d.", 
        VEC_MATRIX_SIZE - 1, LOOP_NUM, process_calc_time, parallel_sum);
}


void matrix_parallel_scan(std::vector<int> inputData)
{
    unsigned long begin_process_time = 0;
    unsigned long end_process_time = 0;

    begin_process_time = getCurrentTimeMsec();
    hetcompute::pscan_inclusive(inputData.begin(), inputData.end(), std::plus<int>());//向量数组初始化
    end_process_time = getCurrentTimeMsec();
    process_calc_time = (float)(end_process_time - begin_process_time);

    HETCOMPUTE_ILOG("matrix_parallel_scan function matrix[x] = matrix[x - 1] + matrix[x]. Consume time: %f ms.", process_calc_time);
}