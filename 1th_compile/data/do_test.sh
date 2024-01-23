#!/bin/bash

list=(omp mpi cuda)
input=(1.in 2.in 3.in)

for i in ${list[@]}; do
    exe=hello_${i}

    if [ ! -f ${exe} ]; then
        exit 1
    fi

    for j in ${input[@]}; do
        in_file=`hpcgame problem-path in/$j`
        ./${exe} $in_file > out.dat
        if [ $? -ne 0 ]; then
            exit 2
        fi
        diff out.dat `hpcgame problem-path out/$j.$i`
        if [ $? -ne 0 ]; then
            exit 3
        fi
    done
done

exit 0
