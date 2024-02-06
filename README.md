# Repository for HPCGame 1st Problems

Welcome to the repository for HPCGame 1st problems.

This repository contains problem descriptions, judgers, data (partial due to Git's limitation on ultra huge files) and answers.

## Building the judgers

To build the judgers (suitable for <https://github.com/lcpu-club/hpcjudge>), please install Golang and Just first.

Next step, just run `just` in the problem's directory, and a tar file will be generated in /dist/. Upload it to the HPC-OJ platform and then it can be used to judge user submissions.

## Finding the answers

The answers are in each problem's `answer` directory. However, for RISC-V problems, we do not provide answers directly due to multiple reasons. These answers will be released soon in another way.
