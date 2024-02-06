#!/bin/bash
mkdir -p /dev/shm/h_mul_$(whoami)
if [ ! -d ./memdisk ]; then
    ln -s /dev/shm/h_mul_$(whoami) ./memdisk
fi
cp /lustre/shared_data/mul/ad1b1b4e3ae715d07720a2c1324485cd/$1 ./memdisk/conf.data
