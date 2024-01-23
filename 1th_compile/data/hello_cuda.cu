#include <cstddef>
#include <fstream>
#include <iostream>

#define MIN_REDUCE_SYNC_SIZE warpSize

// from Parallel and High Performance Computing, Robert Robey
__device__ void reduction_sum_within_block(int *spad) {
  const unsigned int tiX = threadIdx.x;
  const unsigned int ntX = blockDim.x;

  for (int offset = ntX >> 1; offset > MIN_REDUCE_SYNC_SIZE; offset >>= 1) {
    if (tiX < offset) {
      spad[tiX] = spad[tiX] + spad[tiX + offset];
    }
    __syncthreads();
  }
  if (tiX < MIN_REDUCE_SYNC_SIZE) {
    for (int offset = MIN_REDUCE_SYNC_SIZE; offset > 1; offset >>= 1) {
      spad[tiX] = spad[tiX] + spad[tiX + offset];
      __syncthreads();
    }
    spad[tiX] = spad[tiX] + spad[tiX + 1];
  }
}

__global__ void
reduce_sum_stage1of2(const int isize,  // 0  Total number of cells.
                     const int *array, // 1
                     int *blocksum,    // 2
                     int *redscratch)  // 3
{
  extern __shared__ int spad[];
  const unsigned int giX = blockIdx.x * blockDim.x + threadIdx.x;
  const unsigned int tiX = threadIdx.x;

  const unsigned int group_id = blockIdx.x;

  spad[tiX] = 0.0;
  if (giX < isize) {
    spad[tiX] = array[giX];
  }

  __syncthreads();

  reduction_sum_within_block(spad);

  //  Write the local value back to an array size of the number of groups
  if (tiX == 0) {
    redscratch[group_id] = spad[0];
    (*blocksum) = spad[0];
  }
}

__global__ void reduce_sum_stage2of2(const int isize, int *total_sum,
                                     int *redscratch) {
  extern __shared__ int spad[];
  const unsigned int tiX = threadIdx.x;
  const unsigned int ntX = blockDim.x;

  int giX = tiX;

  spad[tiX] = 0.0;

  // load the sum from reduction scratch, redscratch
  if (tiX < isize)
    spad[tiX] = redscratch[giX];

  for (giX += ntX; giX < isize; giX += ntX) {
    spad[tiX] += redscratch[giX];
  }

  __syncthreads();

  reduction_sum_within_block(spad);

  if (tiX == 0) {
    (*total_sum) = spad[0];
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cout << "Usage: " << argv[0] << " <input_file>" << std::endl;
    return 1;
  }

  std::ifstream input_file(argv[1], std::ios::binary);
  int n;
  input_file.read(reinterpret_cast<char *>(&n), sizeof(n));
  int *data = new int[n];
  input_file.read(reinterpret_cast<char *>(data), n * sizeof(int));

  size_t blocksize = 128;
  size_t blocksizebytes = blocksize * sizeof(int);
  size_t global_work_size = ((n + blocksize - 1) / blocksize) * blocksize;
  size_t gridsize = global_work_size / blocksize;

  int *device_data, *device_sum, *device_redscratch;
  cudaMalloc(&device_data, n * sizeof(int));
  cudaMalloc(&device_sum, sizeof(int));
  cudaMalloc(&device_redscratch, gridsize * sizeof(int));

  cudaMemcpy(device_data, data, n * sizeof(int), cudaMemcpyHostToDevice);

  reduce_sum_stage1of2<<<gridsize, blocksize, blocksizebytes>>>(
      n, device_data, device_sum, device_redscratch);

  if (gridsize > 1) {
    reduce_sum_stage2of2<<<1, blocksize, blocksizebytes>>>(n, device_sum,
                                                           device_redscratch);
  }

  int sum;
  cudaMemcpy(&sum, device_sum, sizeof(int), cudaMemcpyDeviceToHost);

  std::cout << "cuda" << std::endl;
  std::cout << sum << std::endl;
  
  cudaFree(device_redscratch);
  cudaFree(device_sum);
  cudaFree(device_data);

  delete[] data;
  return 0;
}
