# 答案

## Makefile

```makefile
all: hello_omp hello_mpi hello_cuda

generate: generate.cpp
    g++ generate.cpp -o generate

hello_omp: hello_omp.cpp
    g++ hello_omp.cpp -o hello_omp -fopenmp

hello_mpi: hello_mpi.cpp
    mpicxx hello_mpi.cpp -o hello_mpi

hello_cuda: hello_cuda.cu
    nvcc hello_cuda.cu -o hello_cuda

handout:
    tar -czvf handout.tar.gz hello_cuda.cu hello_mpi.cpp hello_omp.cpp README.md

clean:
    rm hello_omp hello_mpi hello_cuda
```

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.18)

project(HelloParallel LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 20)

find_package(OpenMP REQUIRED)

add_executable(hello_omp hello_omp.cpp)
target_link_libraries(hello_omp PUBLIC OpenMP::OpenMP_CXX)

find_package(MPI REQUIRED)

add_executable(hello_mpi hello_mpi.cpp)
target_link_libraries(hello_mpi PUBLIC MPI::MPI_CXX)

set(CMAKE_CUDA_ARCHITECTURES 80)
set(CUDA_ARCHITECTURES 80)
enable_language(CUDA)
add_executable(hello_cuda hello_cuda.cu)
```
