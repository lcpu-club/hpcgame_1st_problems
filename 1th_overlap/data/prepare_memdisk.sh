#!/bin/bash
mkdir -p /dev/shm/h_overlap_$(whoami)
if [ ! -d ./memdisk ]; then
    ln -s /dev/shm/h_overlap_$(whoami) ./memdisk
fi
cp /lustre/shared_data/overlap/81bc78e5750d774ca78bff560e799ad/$1 ./memdisk/$1
