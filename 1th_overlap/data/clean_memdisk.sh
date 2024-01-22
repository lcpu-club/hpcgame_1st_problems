#!/bin/bash

if [ -d ./memdisk ]; then
    rm -rf ./memdisk
fi
rm -rf /dev/shm/h_overlap_$(whoami)
