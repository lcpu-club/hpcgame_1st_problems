#include <fstream>
#include <iostream>
#include <mpi.h>

int main(int argc, char *argv[])
{
    int *data, n;
    int rank, nprocs;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    if (rank == 0)
    {
        if (argc < 2)
        {
            std::cout << "Usage: " << argv[0] << " <input_file>" << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        std::ifstream input_file(argv[1], std::ios::binary);
        input_file.read(reinterpret_cast<char *>(&n), sizeof(n));
        data = new int[n];
        input_file.read(reinterpret_cast<char *>(data), n * sizeof(int));
    }

    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int *sendcounts = new int[nprocs];
    int *displs = new int[nprocs];

    int block_size = (n + nprocs - 1) / nprocs;
    int remain_size = n;
    for (int i = 0; i < nprocs; i++)
    {
        sendcounts[i] = std::min(remain_size, block_size);
        displs[i] = n - remain_size;
        remain_size -= sendcounts[i];
    }

    int *curr_data = new int[sendcounts[rank]];

    MPI_Scatterv(data, sendcounts, displs, MPI_INT, curr_data, sendcounts[rank],
                 MPI_INT, 0, MPI_COMM_WORLD);

    int sum = 0;
    for (int i = 0; i < sendcounts[rank]; i++)
    {
        sum += curr_data[i];
    }

    int total_sum = 0;
    MPI_Reduce(&sum, &total_sum, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0)
    {
        std::cout << "mpi" << std::endl;
        std::cout << total_sum << std::endl;
        delete[] data;
    }
    delete[] curr_data;
    MPI_Finalize();
    return 0;
}
