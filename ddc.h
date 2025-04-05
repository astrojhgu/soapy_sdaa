#ifndef DDC_H
#define DDC_H

#include <cuComplex.h>
#include <cassert>
#include <cstdio> // 使用 C++ 风格的头文件

using namespace std;
static constexpr float PI = 3.14159265358979323846;

// DDC 处理所需的 GPU 资源
struct DDCResources
{
    int N; // 每次追加的数据长度
    int M; // 累积多少块数据后计算
    int NDEC;
    int K;
    int16_t *d_indata;
    cuFloatComplex *d_outdata;
    cuFloatComplex *gpu_buffer;
    float *d_fir_coeffs;
    int16_t *h_indata;
    int h_index;
};

void init_ddc_resources(DDCResources *res, int N, int M, int NDEC, int K, const float *fir_coeffs);
void free_ddc_resources(DDCResources *res);
int ddc(const int16_t *indata, int lo_ch, DDCResources *res);
void fetch_output(std::complex<float> *outdata, DDCResources *res);
int calc_output_size(const DDCResources *res);
#endif
