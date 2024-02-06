
#include <iostream>
#include <chrono>

void mul(double *a, double *b, double *c, uint64_t n1, uint64_t n2, uint64_t n3)
{
    for (int i = 0; i < n1; i++)
    {
        for (int j = 0; j < n2; j++)
        {
            for (int k = 0; k < n3; k++)
            {
                c[i * n3 + k] += a[i * n2 + j] * b[j * n3 + k];
            }
        }
    }
}

int main()
{
    uint64_t n1, n2, n3;
    FILE *fi;

    fi = fopen("conf.data", "rb");
    fread(&n1, 1, 8, fi);
    fread(&n2, 1, 8, fi);
    fread(&n3, 1, 8, fi);

    double *a = (double *)malloc(n1 * n2 * 8);
    double *b = (double *)malloc(n2 * n3 * 8);
    double *c = (double *)malloc(n1 * n3 * 8);

    fread(a, 1, n1 * n2 * 8, fi);
    fread(b, 1, n2 * n3 * 8, fi);
    fclose(fi);

    for (uint64_t i = 0; i < n1; i++)
    {
        for (uint64_t k = 0; k < n3; k++)
        {
            c[i * n3 + k] = 0;
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    mul(a, b, c, n1, n2, n3);
    auto t2 = std::chrono::steady_clock::now();
    int d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    printf("%d\n", d1);

    fi = fopen("out.data", "wb");
    fwrite(c, 1, n1 * n3 * 8, fi);
    fclose(fi);

    return 0;
}
