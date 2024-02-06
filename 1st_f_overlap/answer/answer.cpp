#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mpi.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <sys/mman.h>
#include <unistd.h>

#define DIGEST_NAME "SHA512"

namespace fs = std::filesystem;

constexpr size_t BLOCK_SIZE = 1024 * 1024;

void print_checksum(std::ostream &os, uint8_t *md, size_t len);

int get_num_block(int rank, int nprocs, int num_block_total)
{
    return num_block_total / nprocs + ((rank < num_block_total % nprocs) ? 1 : 0);
}

int main(int argc, char *argv[])
{

    MPI_Init(&argc, &argv);

    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    int num_block_total, num_block, fd;
    size_t file_size;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_MD *sha512 = EVP_MD_fetch(nullptr, DIGEST_NAME, nullptr);

    fs::path input_path, output_path;
    uint8_t *file_buffer = nullptr;

    if (rank == 0)
    {
        if (argc < 3)
        {
            std::cout << "Usage: " << argv[0] << " <input_file> <output_file>"
                      << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    input_path = argv[1];
    output_path = argv[2];
    if (rank == 0)
    {
        file_size = fs::file_size(input_path);
        std::cout << input_path << " size: " << file_size << std::endl;
    }

    MPI_Bcast(&file_size, 1, MPI_INT64_T, 0, MPI_COMM_WORLD);
    num_block_total = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    num_block = get_num_block(rank, nprocs, num_block_total);

    MPI_Datatype filetype, contig;
    MPI_Offset disp;
    MPI_Type_contiguous(BLOCK_SIZE, MPI_BYTE, &contig);
    MPI_Type_create_resized(contig, 0, nprocs * BLOCK_SIZE, &filetype);
    MPI_Type_commit(&filetype);

    MPI_Offset offset = rank * BLOCK_SIZE;
    MPI_File fh;

    MPI_File_open(MPI_COMM_WORLD, input_path.c_str(), MPI_MODE_RDONLY,
                  MPI_INFO_NULL, &fh);
    MPI_File_set_view(fh, offset, MPI_BYTE, filetype, "native", MPI_INFO_NULL);

    uint8_t prevdigest[SHA512_DIGEST_LENGTH];
    uint8_t outdigest[SHA512_DIGEST_LENGTH];
    MPI_Request request[2] = {MPI_REQUEST_NULL, MPI_REQUEST_NULL};
    int nblk_index = 0;

    uint8_t curr_block[BLOCK_SIZE];
    for (int i = 0; i < num_block; i++)
    {
        MPI_File_read(fh, curr_block, BLOCK_SIZE, MPI_BYTE, MPI_STATUS_IGNORE);

        unsigned int len = 0;

        if (rank == 0 && i == 0)
        {
            SHA512(nullptr, 0, prevdigest);
        }
        else
        {
            // receive the checksum of prev data block
            MPI_Irecv(prevdigest, SHA512_DIGEST_LENGTH, MPI_BYTE,
                      (rank == 0 ? nprocs - 1 : rank - 1), 1, MPI_COMM_WORLD,
                      &request[0]);
        }

        EVP_DigestInit_ex(ctx, sha512, nullptr);

        EVP_DigestUpdate(ctx, curr_block, BLOCK_SIZE);

        MPI_Waitall(2, request, MPI_STATUSES_IGNORE);

        EVP_DigestUpdate(ctx, prevdigest, SHA512_DIGEST_LENGTH);

        EVP_DigestFinal_ex(ctx, outdigest, &len);

        if (i * nprocs + rank == num_block_total - 1)
        {
            // send the result to rank 0
            MPI_Send(outdigest, SHA512_DIGEST_LENGTH, MPI_BYTE, 0, 2, MPI_COMM_WORLD);
        }
        else
        {
            // send to checksum to the next data block
            MPI_Isend(outdigest, SHA512_DIGEST_LENGTH, MPI_BYTE, (rank + 1) % nprocs,
                      1, MPI_COMM_WORLD, &request[1]);
        }
    }

    MPI_File_close(&fh);

    if (rank == 0 && num_block == 0)
    {
        // deal with the situation that file_size is 0
        SHA512(nullptr, 0, outdigest);
        MPI_Send(outdigest, SHA512_DIGEST_LENGTH, MPI_BYTE, 0, 2, MPI_COMM_WORLD);
    }

    if (rank == 0)
    {
        // receive result
        uint8_t resultdigest[SHA512_DIGEST_LENGTH];
        MPI_Recv(resultdigest, SHA512_DIGEST_LENGTH, MPI_BYTE, MPI_ANY_SOURCE, 2,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        std::ofstream output_file(output_path);
        print_checksum(output_file, resultdigest, SHA512_DIGEST_LENGTH);
    }

    EVP_MD_free(sha512);
    EVP_MD_CTX_free(ctx);

    MPI_Finalize();

    return 0;
}

void print_checksum(std::ostream &os, uint8_t *md, size_t len)
{
    for (int i = 0; i < len; i++)
    {
        os << std::setw(2) << std::setfill('0') << std::hex
           << static_cast<int>(md[i]);
    }
}
