#ifndef _HETCOMPUTE_DSP_H
#define _HETCOMPUTE_DSP_H
/// @file hetcompute_dsp.idl
///
#include "AEEStdDef.h"
#ifndef __QAIC_HEADER
#define __QAIC_HEADER(ff) ff
#endif //__QAIC_HEADER

#ifndef __QAIC_HEADER_EXPORT
#define __QAIC_HEADER_EXPORT
#endif // __QAIC_HEADER_EXPORT

#ifndef __QAIC_HEADER_ATTRIBUTE
#define __QAIC_HEADER_ATTRIBUTE
#endif // __QAIC_HEADER_ATTRIBUTE

#ifndef __QAIC_IMPL
#define __QAIC_IMPL(ff) ff
#endif //__QAIC_IMPL

#ifndef __QAIC_IMPL_EXPORT
#define __QAIC_IMPL_EXPORT
#endif // __QAIC_IMPL_EXPORT

#ifndef __QAIC_IMPL_ATTRIBUTE
#define __QAIC_IMPL_ATTRIBUTE
#endif // __QAIC_IMPL_ATTRIBUTE
#ifdef __cplusplus
extern "C" {
#endif
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_empty_kernel)(void) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_empty_buffer)(const int* vec, int vecLen) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_return_input)(int input) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_div_int)(int dividend, int divisor, int* quotient) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_sum)(const int* vec, int vecLen, int64* res) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_sum_float)(const float* vec, int vecLen, float* res) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_is_prime)(int n, int* prime) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_inout_kernel)(const int* vec, int vecLen, int* outvec, int outvecLen) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_array_is_prime)(const int* numbers, int numbersLen, int* primes, int primesLen) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_fixed_work)(int n, int* accu0, int* accu1) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_increase_modulo_three)(int a, int b, int c, const int* invec, int invecLen, int* outvec, int outvecLen) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_hex_pattern_test_1d)(int first_x, int last_x, const int* input, int inputLen, int* output, int outputLen) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_hex_pattern_test_2d)(int first_x, int last_x, int first_y, int last_y, const int* input, int inputLen, int* output, int outputLen) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_hex_pattern_test_3d)(int first_x, int last_x, int first_y, int last_y, int first_z, int last_z, const int* input, int inputLen, int* output, int outputLen) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_vadd)(int first, int last, const float* avec, int avecLen, const float* bvec, int bvecLen, float* cvec, int cvecLen) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_constant_add)(int first, int last, const float* avec, int avecLen, float* cvec, int cvecLen) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_vwrite)(int first, int last, float* avec, int avecLen, int val) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_matmul)(int first_x, int last_x, int first_y, int last_y, const float* avec, int avecLen, const float* bvec, int bvecLen, float* cvec, int cvecLen, int M, int N, int P) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_math_cbrt)(int first, int last, const float* a, int aLen, float* b, int bLen, int sz) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_matrix_buffer)(const int* matrixA, int matrixALen, int* matrixB, int matrixBLen) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_compute_intensity_dist_weight_table)(float* simTable, int simTableLen) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(hetcompute_dsp_denoise_image_process)(char* input_buffer, int input_buffer_len, float* output_buffer, int output_buffer_len, const float* similarity_weights_buffer, int similarity_weights_buffer_len, const int searchWindowSize, const int similarityWindowSize, const int imgWidth, const int imgHeight, const int width, const int height) __QAIC_HEADER_ATTRIBUTE;
#ifdef __cplusplus
}
#endif
#endif //_HETCOMPUTE_DSP_H
