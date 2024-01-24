#!/bin/bash

module load cuda
moculd load gcc
module load openmpi

`hpcgame problem-path ./judge`
