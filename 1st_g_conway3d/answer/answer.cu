#include "cuda_runtime.h"
#include "device_launch_parameters.h"

#include <stdio.h>
#include <stdint.h>
#include <chrono>
#include <iostream>



template <int size>
__global__ void life(uint32_t* datain, uint32_t* dataout, int xs, int ys, int zs) {
    int y = blockIdx.z;

    __shared__ uint32_t tempsrc[(size / 4) * 4];
    __shared__ uint32_t tempin[(size / 4 + 2) * 6];
    uint8_t* tempsrc_ = (uint8_t*)tempsrc;
    //uint8_t* tempin_ = (uint8_t*)tempin;
    uint32_t tempout[4];

    int z = blockIdx.y;

    int z_[3];
    z_[0] = (z + zs - 1) % zs;
    z_[1] = z;
    z_[2] = (z + 1) % zs;

    uint32_t loc[6];
    for (int j = 0; j < 6; j++) {
        int y_ = y * 4 + ys - 1 + j;
        y_ = y_ % ys;

        loc[j] = 0;
        for (int k = 0; k < 3; k++) {
            int i_ = (z_[k] * ys + y_) * xs + threadIdx.x;

            loc[j] += datain[i_];
        }
    }


    for (int j = 0; j < 4; j++) {
        int y_ = y * 4 + j;

        int i_ = (z * ys + y_) * xs + threadIdx.x;
        uint32_t loc = datain[i_];

        int ini = j * (size / 4) + threadIdx.x;
        tempsrc[ini] = loc;
    }
    __syncthreads();


    loc[0] = loc[0] + loc[1] + loc[2];
    loc[1] = loc[1] + loc[2] + loc[3];
    loc[2] = loc[2] + loc[3] + loc[4];
    loc[3] = loc[3] + loc[4] + loc[5];

    tempin[0 * (size / 4 + 2) + threadIdx.x + 1] = loc[0];
    tempin[1 * (size / 4 + 2) + threadIdx.x + 1] = loc[1];
    tempin[2 * (size / 4 + 2) + threadIdx.x + 1] = loc[2];
    tempin[3 * (size / 4 + 2) + threadIdx.x + 1] = loc[3];
    __syncthreads();


    if (threadIdx.x < 4) {
        tempin[threadIdx.x * (size / 4 + 2) + 0] = tempin[threadIdx.x * (size / 4 + 2) + (size / 4)];
        tempin[threadIdx.x * (size / 4 + 2) + (size / 4 + 1)] = tempin[threadIdx.x * (size / 4 + 2) + 1];
    }
    __syncthreads();


    for (int j = 0; j < 4; j++) {
        uint32_t loc0 = tempin[j * (size / 4 + 2) + threadIdx.x];
        uint32_t loc1 = tempin[j * (size / 4 + 2) + threadIdx.x + 1];
        uint32_t loc2 = tempin[j * (size / 4 + 2) + threadIdx.x + 2];
        loc0 = loc1 + (loc1 >> 8) + (loc2 << 24) + (loc1 << 8) + (loc0 >> 24);


        for (int step = 0; step < 4; step++) {

            uint32_t c = loc0 & 0xff;
            loc0 = loc0 >> 8;

            uint32_t loc = tempsrc_[(j * (size / 4) + threadIdx.x) * 4 + step];
            if (c == 6 || 6 <= c && c <= 8 && loc) {
                tempout[step] = 1;
            }
            else {
                tempout[step] = 0;
            }
        }

        uint32_t out = tempout[0] + (tempout[1] << 8) + (tempout[2] << 16) + (tempout[3] << 24);
        int y_ = y * 4 + j;
        int i_ = (z * ys + y_) * xs + threadIdx.x;
        dataout[i_] = out;
    }
}


void tofile(void* p, size_t n, const char* fn) {
    FILE* fi = fopen(fn, "wb");
    fwrite(p, 1, n, fi);
    fclose(fi);
}

void fromfile(void* p, size_t n, const char* fn) {
    FILE* fi = fopen(fn, "rb");
    fread(p, 1, n, fi);
    fclose(fi);
}

int main(int argc, const char** argv)
{
    if (argc < 4) {
        std::cout << "Usage: " << argv[0] << " <input_path> <output_path> <N>" << std::endl;

        argv = (const char**)malloc(32);
        argv[1] = "../../conf.data";
        argv[2] = "../../out.data";
        argv[3] = "2";
    }

    int itn;
    sscanf(argv[3], "%d", &itn);

    cudaError_t cudaStatus;

    cudaStatus = cudaSetDevice(0);
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "addWithCuda failed!");
    }

    uint32_t* a;
    uint32_t* dev_a = 0;
    uint32_t* dev_b = 0;

    int64_t size;
    int64_t t;

    FILE* fi = fopen(argv[1], "rb");
    fread(&size, 1, 8, fi);
    fread(&t, 1, 8, fi);
    a = (uint32_t*)malloc(size * size * size);
    fread(a, 1, size * size * size, fi);
    fclose(fi);

    cudaStatus = cudaMalloc((void**)&dev_a, size * size * size);
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaMalloc failed!");
    }
    cudaStatus = cudaMalloc((void**)&dev_b, size * size * size);
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaMalloc failed!");
    }

    cudaStatus = cudaMemcpy(dev_a, a, size * size * size, cudaMemcpyHostToDevice);
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "copy failed!");
    }

    dim3 g = dim3(1, size, size / 4);
    dim3 b = dim3(size / 4, 1, 1);

    auto t1 = std::chrono::steady_clock::now();

    if (size == 256) {
        for (int i = 0; i < itn / 2; i++) {
            life<256> << <g, b >> > (dev_a, dev_b, size / 4, size, size);
            life<256> << <g, b >> > (dev_b, dev_a, size / 4, size, size);
        }
    }
    else if (size == 512) {
        for (int i = 0; i < itn / 2; i++) {
            life<512> << <g, b >> > (dev_a, dev_b, size / 4, size, size);
            life<512> << <g, b >> > (dev_b, dev_a, size / 4, size, size);
        }
    }
    else if (size == 1024) {
        for (int i = 0; i < itn / 2; i++) {
            life<1024> << <g, b >> > (dev_a, dev_b, size / 4, size, size);
            life<1024> << <g, b >> > (dev_b, dev_a, size / 4, size, size);
        }
    }

    cudaStatus = cudaDeviceSynchronize();
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "calc failed");
    }
    auto t2 = std::chrono::steady_clock::now();

    cudaStatus = cudaMemcpy(a, dev_a, size * size * size, cudaMemcpyDeviceToHost);
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaMalloc failed!");
    }

    t += itn;
    fi = fopen(argv[2], "wb");
    fwrite(&size, 1, 8, fi);
    fwrite(&t, 1, 8, fi);
    fwrite(a, 1, size * size * size, fi);
    fclose(fi);

    int d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    printf("%d\n", d1);

    cudaStatus = cudaDeviceReset();
    if (cudaStatus != cudaSuccess) {
        fprintf(stderr, "cudaDeviceReset failed!");
    }

    return 0;
}
