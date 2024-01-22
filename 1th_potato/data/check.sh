#!/bin/bash

nodes=$(LD_PRELOAD=/usr/lib64/slurm/libslurmfull.so seff $1 | grep Nodes | sed 's/Nodes: //')
tasks_per_node=$(LD_PRELOAD=/usr/lib64/slurm/libslurmfull.so seff $1 | grep "Cores per node:" | sed 's/Cores per node: //')
user=$(LD_PRELOAD=/usr/lib64/slurm/libslurmfull.so seff $1 | grep User/Group | sed 's/\// /g' | awk '{ print $3 }')

echo $user

if [[ $nodes -ne 4 || $tasks_per_node -ne 4 ]]; then
    exit 1
fi

exit 0
