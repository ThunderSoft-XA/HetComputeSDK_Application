/*==============================================================================
  Copyright (c) 2012-2014 Qualcomm Technologies, Inc.
  All rights reserved. Qualcomm Proprietary and Confidential.
==============================================================================*/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "HAP_farf.h"
#include "hetcompute_dsp.h"
#include "macrodefinitions_dsp_1d.h"
#include "macrodefinitions_dsp_2d.h"

struct Point
{
    int x; // the col, width;
    int y; // the row, height
};

int
hetcompute_dsp_empty_kernel()
{
    return 0;
}

///////////////////////////////////////////////////////////////////////////////

int
hetcompute_dsp_empty_buffer(const int* vec, int vecLen)
{
    return 0;
}

///////////////////////////////////////////////////////////////////////////////

/* used to verify hetcompute exceptions */
int
hetcompute_dsp_return_input(const int input)
{
    return input;
}

///////////////////////////////////////////////////////////////////////////////

int
hetcompute_dsp_div_int(const int dividend, const int divisor, int* quotient)
{
    *quotient = dividend / divisor;
    FARF(HIGH, "===============     DSP: int div result %lld ===============", *quotient);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////

int
hetcompute_dsp_sum(const int* vec, int vecLen, int64* res)
{
    int ii = 0;
    *res   = 0;
    for (ii = 0; ii < vecLen; ++ii)
    {
        *res = *res + vec[ii];
    }
    FARF(HIGH, "===============     DSP: int sum result %lld ===============", *res);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////

int
hetcompute_dsp_sum_float(const float* vec, int vecLen, float* res)
{
    int ii = 0;
    *res   = 0.0;
    for (ii = 0; ii < vecLen; ++ii)
    {
        *res = *res + vec[ii];
    }
    FARF(HIGH, "===============     DSP: float sum result %f ===============", *res);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////

// This function computes if number n is prime.
// It sets prime to 1 when n is prime and to 0 otherwise
int
hetcompute_dsp_is_prime(const int n, int* prime)
{
    if ((n & 0x1) == 0)
    {
        *prime = 0;
        return 0;
    }

    // assume the value is prime
    long _prime = 1;
    int  m      = (int)sqrt(n);

    // only check for odd numbers
    for (int i = 3; (i <= m) && (_prime == 1); i += 2)
    {
        _prime = (n % i) == 0;
    }

    *prime = _prime;

    FARF(HIGH, "===============     DSP: is %d prime? %d    ===============", n, *prime);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////

int
hetcompute_dsp_array_is_prime(const int* numbers, int numbersLen, int* primes, int primesLen)
{
    int i;

    for (i = 0; (i < numbersLen) && (i < primesLen); i++)
    {
        hetcompute_dsp_is_prime(numbers[i], &primes[i]);
        FARF(HIGH, "===============     DSP: array : is %d prime? %d    ===============", numbers[i], primes[i]);
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////

int
hetcompute_dsp_inout_kernel(const int* vec, int vecLen, int* outvec, int outvecLen)
{
    int i;
    for (i = 0; i < vecLen; i++)
    {
        outvec[i] = vec[i];
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////

int
hetcompute_dsp_fixed_work(int iters, int* accu0, int* accu1)
{
    int _accu0 = 0, _accu1 = 1;
    int v, s;

    for (int i = 0; i < iters; ++i)
    {
        v = 3;
        s = 4;
        asm volatile("" : "+r"(v), "+r"(s));
        if ((iters & 0x1) && (i & 0x1))
        {
            _accu0 += (v + s);
            _accu1 += (v - s);
        }
        else
        {
            _accu0 -= (v - s);
            _accu1 -= (v + s);
        }
    }
    *accu0 = _accu0;
    *accu1 = _accu1;
    return 0;
}

///////////////////////////////////////////////////////////////////////////////

int
hetcompute_dsp_increase_modulo_three(int a, int b, int c, const int* invec, int invec_length, int* outvec, int outvec_length)
{
    for (int i = 0; (i < invec_length) && (i < outvec_length); i++)
    {
        switch (i % 3)
        {
        case 0:
        {
            outvec[i] = invec[i] + a;
            break;
        }
        case 1:
        {
            outvec[i] = invec[i] + b;
            break;
        }
        case 2:
        {
            outvec[i] = invec[i] + c;
            break;
        }
        default:
        {
            break;
        }
        }
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////

// functions for hetero patterntest
int
hetcompute_dsp_hex_pattern_test_1d(const int first_x, const int last_x, const int* input, int inputlen, int* output, int outputlen)
{
    int i;
    for (i = first_x; i < last_x; i++)
    {
        output[i] = 2 * input[i];
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////

int
hetcompute_dsp_hex_pattern_test_2d(const int  first_x,
                                   const int  last_x,
                                   const int  first_y,
                                   const int  last_y,
                                   const int* input,
                                   int        inputlen,
                                   int*       output,
                                   int        outputlen)
{
    int i, j;
    int height = last_y - first_y;
    for (i = first_x; i < last_x; i++)
    {
        for (j = first_y; j < last_y; j++)
        {
            int idx     = i * height + j;
            output[idx] = 2 * input[idx];
        }
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////

int
hetcompute_dsp_hex_pattern_test_3d(const int  first_x,
                                   const int  last_x,
                                   const int  first_y,
                                   const int  last_y,
                                   const int  first_z,
                                   const int  last_z,
                                   const int* input,
                                   int        inputlen,
                                   int*       output,
                                   int        outputlen)
{
    int i, j, k;
    int height = last_y - first_y;
    int depth  = last_z - first_z;
    for (i = first_x; i < last_x; i++)
    {
        for (j = first_y; j < last_y; j++)
        {
            for (k = first_z; k < last_z; k++)
            {
                int idx     = i * height * depth + j * depth + k;
                output[idx] = 2 * input[idx];
            }
        }
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////

int 
hetcompute_dsp_matrix_buffer(const int* matrixA, int matrixALen, 
			                       int* matrixB, int matrixBLen)
{
    // matrix addition and multiplication
    for (size_t x = 0; (x < matrixALen) && (x < matrixBLen); x++) {
        matrixB[x] = matrixA[x] + matrixA[x];
        matrixB[x] = matrixA[x] * matrixA[x];
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////

int
hetcompute_dsp_compute_intensity_dist_weight_table(float* simTable, int simTableLen)
{
    const double WEIGHT_THRESHOLD = 0.001f;
    float h = 15.0f;

    for (int dist = 0; dist < simTableLen; dist++) {
        float w = exp(-dist / (h * h));

        if (w < WEIGHT_THRESHOLD) {
            w = 0;
        }

        simTable[dist] = w;
    }

    return 0;
}

void
hetcompute_dsp_clamp_to_reflection(struct Point* p, 
                                   const int imgWidth, 
                                   const int imgHeight)
{
    p->x = abs(p->x);
    p->y = abs(p->y);

    if (p->x >= imgWidth) {
        p->x = imgWidth - (p->x - imgWidth) - 1;
    }

    if (p->y >= imgHeight) {
        p->y = imgHeight - (p->y - imgHeight) - 1;
    }
}

int
hetcompute_dsp_get_neighbor_dist_to_current(struct Point ref, 
                                            struct Point p, 
                                            const char* input_buffer, 
                                            const int similarityWindowSize, 
                                            const int imgWidth, 
                                            const int imgHeight)
{
    int area = similarityWindowSize * similarityWindowSize;
    int color_dist_sum = 0;
    struct Point p_neighbor;
    struct Point ref_neighbor;

    for (int i = 0; i < similarityWindowSize; i++) {
        for (int j = 0; j < similarityWindowSize; j++) {
            p_neighbor.x = p.x - similarityWindowSize / 2 + j;
            p_neighbor.y = p.y - similarityWindowSize / 2 + i;
            hetcompute_dsp_clamp_to_reflection(&p_neighbor, imgWidth, imgHeight);

            ref_neighbor.x = ref.x - similarityWindowSize / 2 + j;
            ref_neighbor.y = ref.y - similarityWindowSize / 2 + i;
            hetcompute_dsp_clamp_to_reflection(&ref_neighbor, imgWidth, imgHeight);

            int dist = input_buffer[p_neighbor.y * imgWidth + p_neighbor.x] - input_buffer[ref_neighbor.y * imgWidth + ref_neighbor.x];
            color_dist_sum += dist * dist;
        }
    }

    color_dist_sum /= area;
    return color_dist_sum;
}

void
hetcompute_dsp_compute_weights(const char* input_buffer, 
                               struct Point point, 
                               float* reusable_buffer, 
                               const float* similarity_weights_buffer, 
                               const int searchWindowSize, 
                               const int similarityWindowSize, 
                               const int imgWidth, 
                               const int imgHeight)
{
    for (size_t i = 0; i < searchWindowSize; i++) {
        for (size_t j = 0; j < searchWindowSize; j++) {
            struct Point neighbor;
            neighbor.x = point.x - searchWindowSize / 2 + i;
            neighbor.y = point.y - searchWindowSize / 2 + j;
            hetcompute_dsp_clamp_to_reflection(&neighbor, imgWidth, imgHeight);

            int dist = hetcompute_dsp_get_neighbor_dist_to_current(point, neighbor, input_buffer, similarityWindowSize, imgWidth, imgHeight);
            reusable_buffer[i * searchWindowSize + j] = similarity_weights_buffer[dist];
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

int 
hetcompute_dsp_denoise_image_process(const char* input_buffer, int input_buffer_len, 
                                     float* output_buffer, int output_buffer_len, 
                                     const float* similarity_weights_buffer, int similarity_weights_buffer_len, 
                                     const int searchWindowSize, const int similarityWindowSize, 
                                     const int imgWidth, const int imgHeight, 
                                     const int width, const int height)
{
    float reusable_buffer[searchWindowSize * searchWindowSize];
    struct Point point;
    point.x = width;
    point.y = height;
    // Compute weights for points in the search window.
    hetcompute_dsp_compute_weights(input_buffer, point, reusable_buffer, similarity_weights_buffer, searchWindowSize, similarityWindowSize, imgWidth, imgHeight);

    float weight_sum = 0;
    float temp = 0;

    for (int m = 0; m < searchWindowSize; m++) {
        for (int n = 0; n < searchWindowSize; n++) {
            struct Point search_neighbor;

            search_neighbor.x = width - searchWindowSize / 2 + m;
            search_neighbor.y = height - searchWindowSize / 2 + n;
            hetcompute_dsp_clamp_to_reflection(&search_neighbor, imgWidth, imgHeight);

            temp += reusable_buffer[m * searchWindowSize + n] * input_buffer[search_neighbor.y * imgWidth + search_neighbor.x];
            weight_sum += reusable_buffer[m * searchWindowSize + n];
        }
    }

    temp /= weight_sum;
    output_buffer[height * imgWidth + width] = temp;

    return 0;
}

///////////////////////////////////////////////////////////////////////////////

HETCOMPUTE_POINT_KERNEL_1D_6(vadd, int, i, first, last,
                           const float*, a, int, num_elemsa,
                           const float*, b, int, num_elemsb,
                           float*, c, int, num_elemsc,
                           { c[i] = a[i] + b[i]; });

///////////////////////////////////////////////////////////////////////////////

HETCOMPUTE_POINT_KERNEL_1D_4(constant_add, int, i, first, last,
                           const float*, a, int, num_elemsa,
                           float*, b, int, num_elemsb,
                           { b[i] = a[i] + 100; });

///////////////////////////////////////////////////////////////////////////////

HETCOMPUTE_POINT_KERNEL_1D_3(vwrite, int, i, first, last,
                           float*, a, int, size, int, n,
                           {
                             if(i < size)
                               a[i] = n;
                           });

///////////////////////////////////////////////////////////////////////////////

HETCOMPUTE_POINT_KERNEL_2D_9(matmul, int, i,j, first_x, last_x, first_y, last_y,
                           const float*, a, int, num_elemsa,
                           const float*, b, int, num_elemsb,
                           float*, c, int, num_elemsc,
                           int, M, int, P, int, N,
                           {
                             if (i < M && j < N){
                               c[i * N + j] = 0;
                               for (int k = 0; k < P; k++) {
                                  c[i * N + j] += a[i * P + k] * b[k * N + j];
                               }
                             }
                           });

///////////////////////////////////////////////////////////////////////////////

HETCOMPUTE_POINT_KERNEL_1D_5(math_cbrt, int, i, first, last,
                           const float*, a, int, num_elemsa,
                           float*, b, int, num_elemsb,
                           int, sz,
                           {
                             if (i < sz) {
                               b[i] = cbrtf(a[i]);
                             }
                           });

///////////////////////////////////////////////////////////////////////////////
