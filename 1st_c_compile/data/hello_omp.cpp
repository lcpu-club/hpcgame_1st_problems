#include <fstream>
#include <iostream>
#include <omp.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " <input_file>" << std::endl;
        return 1;
    }

    // check if openmp is used
    int num_threads = omp_get_max_threads();
    if (num_threads == 1)
    {
        std::cout << "OpenMP is not used" << std::endl;
        return 1;
    }

    std::ifstream input_file(argv[1], std::ios::binary);
    int n;
    input_file.read(reinterpret_cast<char *>(&n), sizeof(n));
    int *data = new int[n];
    input_file.read(reinterpret_cast<char *>(data), n * sizeof(int));

    int sum = 0;
#pragma omp parallel for reduction(+ : sum)
    for (int i = 0; i < n; i++)
    {
        sum += data[i];
    }

    std::cout << "openmp" << std::endl;
    std::cout << sum << std::endl;

    delete[] data;
    return 0;
}
