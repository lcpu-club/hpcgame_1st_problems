#include <iostream>
#include <chrono>
#include <omp.h>
#include <immintrin.h>
#include <string.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

constexpr int rb = 32;
constexpr int nb = 8;
constexpr int cb = nb * rb;

void mul32ker(double *__restrict a, double *__restrict b, double *__restrict c)
{
    for (int i = 0; i < rb; i += 32)
    {
        for (int j = 0; j < rb; j += 4)
        {
            __m512d ar;
            __m512d br[4];
            __m512d cr[16];
            for (int m = 0; m < 4; m++)
            {
                cr[m * 4] = _mm512_loadu_pd(c + (j + m) * rb + i);
                cr[m * 4 + 1] = _mm512_loadu_pd(c + (j + m) * rb + i + 8);
                cr[m * 4 + 2] = _mm512_loadu_pd(c + (j + m) * rb + i + 16);
                cr[m * 4 + 3] = _mm512_loadu_pd(c + (j + m) * rb + i + 24);
            }

            for (int k = 0; k < rb; k++)
            {
                br[0] = _mm512_loadu_pd(b + k * rb + i);
                br[1] = _mm512_loadu_pd(b + k * rb + i + 8);
                br[2] = _mm512_loadu_pd(b + k * rb + i + 16);
                br[3] = _mm512_loadu_pd(b + k * rb + i + 24);
                for (int m = 0; m < 4; m++)
                {
                    ar = _mm512_set1_pd(a[j * rb + m * rb + k]);
                    cr[m * 4] = _mm512_fmadd_pd(ar, br[0], cr[m * 4]);
                    cr[m * 4 + 1] = _mm512_fmadd_pd(ar, br[1], cr[m * 4 + 1]);
                    cr[m * 4 + 2] = _mm512_fmadd_pd(ar, br[2], cr[m * 4 + 2]);
                    cr[m * 4 + 3] = _mm512_fmadd_pd(ar, br[3], cr[m * 4 + 3]);
                }
            }

            for (int m = 0; m < 4; m++)
            {
                _mm512_storeu_pd(c + (j + m) * rb + i, cr[m * 4]);
                _mm512_storeu_pd(c + (j + m) * rb + i + 8, cr[m * 4 + 1]);
                _mm512_storeu_pd(c + (j + m) * rb + i + 16, cr[m * 4 + 2]);
                _mm512_storeu_pd(c + (j + m) * rb + i + 24, cr[m * 4 + 3]);
            }
        }
    }
}

void mulcb(double *__restrict a, double *__restrict b, double *__restrict c)
{
    for (int i = 0; i < nb; i++)
    {
        for (int j = 0; j < nb; j++)
        {
            double *c_ = c + i * rb * cb + j * rb * rb;
            for (int k = 0; k < nb; k++)
            {
                double *a_ = a + i * rb * cb + k * rb * rb;
                double *b_ = b + k * rb * cb + j * rb * rb;
                mul32ker(a_, b_, c_);
            }
        }
    }
}

void moverb(double *__restrict dst, double *__restrict src)
{
    _mm512_storeu_pd(dst, _mm512_loadu_pd(src));
    _mm512_storeu_pd(dst + 8, _mm512_loadu_pd(src + 8));
    _mm512_storeu_pd(dst + 16, _mm512_loadu_pd(src + 16));
    _mm512_storeu_pd(dst + 24, _mm512_loadu_pd(src + 24));
}

void loadcb(double *__restrict dst, double *__restrict src, uint64_t n)
{
    for (uint64_t i = 0; i < nb; i++)
    {
        for (uint64_t m = 0; m < rb; m++)
        {
            for (uint64_t j = 0; j < nb; j++)
            {
                moverb(dst + i * rb * cb + j * rb * rb + m * rb, src + (i * rb + m) * n + j * rb);
            }
        }
    }
}

void storecb(double *__restrict dst, double *__restrict src, uint64_t n)
{
    for (uint64_t i = 0; i < nb; i++)
    {
        for (uint64_t m = 0; m < rb; m++)
        {
            for (uint64_t j = 0; j < nb; j++)
            {
                moverb(dst + (i * rb + m) * n + j * rb, src + i * rb * cb + j * rb * rb + m * rb);
            }
        }
    }
}

void mul(double *__restrict a, double *__restrict b, double *__restrict c, uint64_t n1, uint64_t n2, uint64_t n3)
{
    int tn = omp_get_max_threads();
    double *ws = (double *)_mm_malloc(cb * cb * 8 * 3 * tn, 64);

#pragma omp parallel
    {
        int n = n1 / cb * n3 / cb;
        int ti = omp_get_thread_num();
        size_t is = n * ti / tn;
        size_t ie = n * (ti + 1) / tn;

        double *ws_ = ws + cb * cb * 3 * ti;
        double *a_l, *b_l, *c_l;
        a_l = ws_;
        b_l = ws_ + cb * cb;
        c_l = ws_ + 2 * cb * cb;

        for (uint64_t it = is; it < ie; it++)
        {
            uint64_t i = it / (n3 / cb) * cb;
            uint64_t k = it % (n3 / cb) * cb;

            double *__restrict c_ = c + i * n3 + k;
            loadcb(c_l, c_, n3);
            for (uint64_t j = 0; j < n2; j += cb)
            {

                double *__restrict a_ = a + i * n2 + j;
                double *__restrict b_ = b + j * n3 + k;

                loadcb(a_l, a_, n2);
                loadcb(b_l, b_, n3);

                mulcb(a_l, b_l, c_l);
            }
            storecb(c_, c_l, n3);
        }
    }
}

int main()
{
    auto t1 = std::chrono::steady_clock::now();

    size_t n1, n2, n3;
    FILE *fi;

    /*
        fi = fopen("conf.data", "rb");
        fread(&n1, 1, 8, fi);
        fread(&n2, 1, 8, fi);
        fread(&n3, 1, 8, fi);
    */

    int fd = open("conf.data", O_RDONLY);
    size_t len = lseek(fd, 0, SEEK_END);
    printf("%d,%ld\n", fd, len);
    char *addr = (char *)mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
    printf("%d,%x\n", fd, addr);
    n1 = *(size_t *)(addr);
    n2 = *(size_t *)(addr + 8);
    n3 = *(size_t *)(addr + 16);
    printf("%d,%d,%d\n", n1, n2, n3);

    double *a = (double *)_mm_malloc(n1 * n2 * 8, 64);
    double *b = (double *)_mm_malloc(n2 * n3 * 8, 64);
    double *c = (double *)_mm_malloc(n1 * n3 * 8, 64);
    printf("%x,%x,%x\n", a, b, c);

#pragma omp parallel for
    for (int64_t i = 0; i < n1 * n2; i++)
    {
        a[i] = ((double *)(addr + 24))[i];
    }

#pragma omp parallel for
    for (int64_t i = 0; i < n2 * n3; i++)
    {
        b[i] = ((double *)(addr + 24 + n1 * n2 * 8))[i];
    }
    munmap(addr, len);
    close(fd);

    // fread(a, 1, n1 * n2 * 8, fi);
    // fread(b, 1, n2 * n3 * 8, fi);
    // fclose(fi);

    auto t2 = std::chrono::steady_clock::now();

#pragma omp parallel for
    for (uint64_t i = 0; i < n1 * n3; i++)
    {
        c[i] = 0;
    }

    auto t3 = std::chrono::steady_clock::now();
    mul(a, b, c, n1, n2, n3);
    auto t4 = std::chrono::steady_clock::now();

    // fi = fopen("out.data", "wb");
    // fwrite(c, 1, n1 * n3 * 8, fi);
    // fclose(fi);

    fd = open("out.data", O_RDWR | O_CREAT, 00644);
    ftruncate(fd, n1 * n3 * 8);
    addr = (char *)mmap(NULL, n1 * n3 * 8, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    // printf("%d,%x\n",fd,addr);
#pragma omp parallel for
    for (uint64_t i = 0; i < n1 * n3; i++)
    {
        ((double *)(addr))[i] = c[i];
    }
    munmap(addr, n1 * n3 * 8);
    close(fd);

    auto t5 = std::chrono::steady_clock::now();

    int d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    int d2 = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
    int d3 = std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count();
    int d4 = std::chrono::duration_cast<std::chrono::milliseconds>(t5 - t4).count();
    printf("%d,%d,%d,%d\n", d1, d2, d3, d4);

    return 0;
}
