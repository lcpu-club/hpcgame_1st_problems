#include <iostream>
#include <chrono>

template <int n>
void it(double r, double *x, int64_t itn)
{
    for (int64_t i = 0; i < itn; i++)
    {
        for (int64_t j = 0; j < n; j++)
        {
            x[j] = r * x[j] * (1.0 - x[j]);
        }
    }
}

template <int gn>
void itvg(double r, double *x, int64_t n, int64_t itn)
{

#pragma omp parallel for
    for (int64_t i = 0; i < n; i += gn)
    {
        it<gn>(r, x + i, itn);
    }
}

int main()
{

    FILE *fi;
    fi = fopen("conf.data", "rb");

    int64_t itn;
    double r;
    int64_t n;
    double *x;

    fread(&itn, 1, 8, fi);
    fread(&r, 1, 8, fi);
    fread(&n, 1, 8, fi);
    x = (double *)malloc(n * 8);
    fread(x, 1, n * 8, fi);
    fclose(fi);

    auto t1 = std::chrono::steady_clock::now();
    itvg<128>(r, x, n, itn);
    auto t2 = std::chrono::steady_clock::now();
    int d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    printf("%d\n", d1);

    fi = fopen("out.data", "wb");
    fwrite(x, 1, n * 8, fi);
    fclose(fi);

    return 0;
}
