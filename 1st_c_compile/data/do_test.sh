#!/bin/bash

list=(cuda mpi omp)
input=(1.in)

for i in ${list[@]}; do
    exe=hello_${i}
    echo "Testing ${exe}..."

    if [ ! -f ${exe} ]; then
        echo "File ${exe} not found."
        exit 1
    fi

    for j in ${input[@]}; do
        in_file=`hpcgame problem-path in/$j`
        echo "Testing ${exe} with ${in_file}..."
        if [ $i = "cuda" ]; then
            echo "CUDA RUN"
            ./${exe} $in_file > out.dat
        elif [ $i = "mpi" ]; then
            echo "MPI RUN"
            mpirun -np 1 ./${exe} $in_file > out.dat
        elif [ $i = "omp" ]; then
            echo "OMP RUN"
            OMP_NUM_THREADS=2 ./${exe} $in_file > out.dat
        fi
        if [ $? -ne 0 ]; then
            cat out.dat
            rm out.dat
            echo "Execution ${exe} failed."
            exit 2
        fi
        diff out.dat `hpcgame problem-path ans/$j.$i`
        if [ $? -ne 0 ]; then
            cat out.dat
            rm out.dat
            echo "Output of ${exe} is not correct."
            exit 3
        fi
        rm out.dat
    done
done

exit 0
