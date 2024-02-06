# M. RISC-V OpenBLAS

## 题面

### 任务描述

RISC-V 是一种开放授权的精简指令集，目前在体系结构等领域广受欢迎。究其原因，有如下几点：

- 开源开放：任何人都可以免费使用这套指令集来设计、制造和销售 RISC-V 芯片，不用支付授权费用。
- 可定制性和灵活性：RISC-V 允许设计者根据自己的需要添加或修改指令，并以指令集扩展的形式体现
- 促进创新：开放和灵活的特性使得 RISC-V 成为学术研究和创新实验的热门平台，学者和工程师可以自由地试验新的设计思路和优化方法

前几年，RISC-V还是纸上谈兵，大部分的硬件都是模拟器或者小型开发板，并非能真实使用的物理硬件。但到目前，由于许多开源 RISC-V 实现（如[香山](https://github.com/OpenXiangShan/XiangShan/blob/master/readme.zh-cn.md)、[玄铁](https://ieeexplore.ieee.org/document/9138983)）的出现，
众多可用的RISC-V硬件也横空出世。特别是`SG2042`等高核心数，支持DDR4内存和PCIE等功能的“通用RISC-V”主机出现，让我们看到了 RISC-V blob

但同时，开源的坏处就是，没有人为此负责。RISC-V的生态目前还是亟待解决的问题，特别是在高性能计算领域，毕竟很少有人想过 RISC-V还能用来进行高性能计算。目前数值计算最重要的开源库之一`OpenBLAS`在节点上无法得到正确结果，这导致了很多麻烦的问题。

本题的目标是，解决 `OpenBLAS`在RVV-0.7.1向量扩展上的计算错误问题，附件为`baseline`代码

本题在排行榜总分中占 50 分，但本题参与 `RISC-V`赛道的赛道奖励，权重为 50 %。同时，本题也是下一题的重要前提条件。

### 评分标准

#### 机器环境

在登录节点可访问，命令如下：

- `rv_machine init` 初始化
- `rv_machine start` 开机
- `rv_machine shell` 登录

系统为RevyOS，CPU数为64

#### 编译运行 （20%）

提交打包的`OpenBLAS`后编译文件。由评测机运行，结果正确即得分，不要求带向量扩展编译。我们会使用官方测试点和HPL进行正确性测试，附件中有HPL的编译文件。

请打包为`tar.gz`格式，使用`tar xf openblas.tar.gz`后应得到如下的目录结构：

```
OpenBLAS
-- bin（空文件夹，可选）
-- include
    -- cblas.h
    -- ...
-- lib
    -- libopenblas.a
    -- ...
```

#### 向量化正确性（60%）

选手提交源代码文件夹打包得到的`.tar.gz`文件。该文件夹通过`tar xf openblas.tar.gz`解压后应该得到如下目录结构：

```
OpenBLAS
-- Makefile
-- common.h
....
```

我们将使用如下命令编译你的程序，并获得库文件和头文件。我们将进行向量化测试，同时将读取二进制，确认是否使用了向量指令集。正确性测试方法同第一部分。

Hint: OpenBLAS目前有个测试点是fail的

```bash
make clean
make TARGET=C910V CC=riscv64-linux-gnu-gcc-10 FC=riscv64-linux-gnu-gfortran-10
```

#### 性能测试（20%）

我们将使用矩阵乘法测试`OpenBLAS`的性能，你将像提交一个如第一题的压缩包，我们使用如下程序进行性能测试。编译命令：

```bash
g++ --std=c++17 -march=rv64gv0p7_zfh_xtheadc -O3 -funroll-all-loops -finline-limit=500 -fgcse-sm -fno-schedule-insns  -msignedness-cmpiv -fno-code-hoisting -mno-thread-jumps1 -mno-iv-adjust-addr-cost -mno-expand-split-imm test.cpp -o test
```

```cpp
#include <stdio.h>
#include <chrono>

#include <iostream>
#include <random>

// openblas
#include <cblas.h>

int main()
{
    int n = 2048;

    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_real_distribution<double> distribution(0.0,1.0);

    double* a = (double*)malloc(n * n * 8);
    double* b = (double*)malloc(n * n * 8);
    double* c = (double*)malloc(n * n * 8);
    for (uint64_t i = 0; i < n; i++) {
        for (uint64_t k = 0; k < n; k++) {
            a[i * n + k] = distribution(rng);
            b[i * n + k] = distribution(rng);
            c[i * n + k] = distribution(rng);
        }
    }

    for (int i = 0; i < 1; i++) {
        auto t3 = std::chrono::steady_clock::now();
        cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, n, n, n, 1, a, n, b, n, 0, c, n);
        auto t4 = std::chrono::steady_clock::now();
        int d3 = std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count();
        printf("%d\n",d3);
    }
    free(a);
    free(b);
    free(c);
}

```

### 赛道奖励说明

RISC-V 赛道共两道题目，奖金总额两万元。由有有效成绩的选手中的前 20 位进行奖金分配。若一名选手的成绩为 $x$，在奖金分配中的权重即为 $x^2$。奖金额度低于 200 元的选手，奖金将以自选礼奖品的形式发放。

### 测试机器使用方法

我们给每位选手提供测试机。请先登录 SCOW 平台，然后打开一个交互式终端。

我们在登录节点中部署了一个名为 `rv_machine` 的工具，通过这个工具，您可以轻松连接到我们提供的 RISC-V 测试机器。

您可以通过 `rv_machine init` 命令初始化分配给您的测试机。如需重新初始化（例如：您想要清空其中文件，恢复到初始状态），请先运行 `rv_machine delete`。

您可以通过 `rv_machine start` 命令启动测试机。与之对应地，`rv_machine stop` 可用来关闭测试机。

您可以通过 `rv_machine shell` 连接到测试机并启动默认 Shell（通常为 Bash）。如果您只需运行单条命令，可以使用 `rv_machine exec <命令>` 的方式。

您可以通过 `rv_machine status` 查看测试机的状态。如果您需要确认测试机的配置信息，可以运行 `rv_machine info`。

如果您需要向测试机上传/从测试机下载文件，您可运行 `rv_machine upload <本地路径> <远程绝对路径>` 或者 `rv_machine download <远程绝对路径> <本地路径>`。

另外：该 RISC-V 集群具有已知的安全性问题，但我们暂时来不及解决；本比赛非 CTF，如您发现漏洞，可以向我们报告，但是不会获得任何分数奖励；**如您对比赛设施攻击、渗透等，我们保留取消参赛资格乃至追究法律责任的权利**。

### 评测说明

由于测试 RISC-V 集群稳定性较为欠缺，我们决定 RISC-V 相关问题采取人工离线评测的方式，以避免得分不稳定等情况。您在提交后看不到分数，这是正常现象。

每位选手可多次提交答案，但我们将只对最后一次提交进行评测。我们会在比赛结束后的三个工作日内完成所有评测工作。感谢大家的支持！

### 一些知识介绍

#### RISC-V 指令集扩展

（改变自：北京大学编译原理课程，[编译实践在线文档](https://pku-minic.github.io/online-doc/#/lv0-env-config/riscv)）

RISC-V 的指令系统由基础指令系统 (base instruction set) 和指令系统扩展 (extension) 构成. 每个 RISC-V 处理器必须实现基础指令系统, 同时可以支持若干扩展. 常用的基础指令系统有两种:

- RV32I: 32 位整数指令系统.
- RV64I: 64 位整数指令系统. 兼容 RV32I.

常用的标准指令系统扩展包括:

- M 扩展: 包括乘法和除法相关的指令.
- A 扩展: 包括原子内存操作相关的指令.
- F 扩展: 包括单精度浮点操作相关的指令.
- D 扩展: 包括双精度浮点操作相关的指令.
- G 扩展：`IMAFD` 的缩写
- C 扩展: 包括常用指令的 16 位宽度的压缩版本.
- V 扩展：包括向量指令集的指令.

我们通常使用 RV32/64I + 扩展名称的方式来描述某个处理器/平台支持的 RISC-V 指令系统类型, 例如 RV32IMA 代表这个处理器是一个 32 位的, 支持 M 和 A 扩展的 RISC-V 处理器.

本题的评测和实验环境是算能SG2042处理器，扩展是`RV64GCV`，其中V扩展的版本是0.7.1，并非正式版的`RVV` 1.0（在芯片设计的时候还未通过），所以整套工具链较为奇怪。

#### RevyOS

基于 `Debian 12` 的，为 RISC-V 定制的操作系统。因为目前 `RISC-V` 还没有被主流操作系统广泛支持，

其中有 `THead` 工具链，也就是平头哥为 `RISC-V` 0.7 向量化指令集以及 `THead` 扩展提供的 `gcc` 工具链。使用 `gcc-10` 编译时，编译器将进行自动向量化。`gcc-10` 为默认编译器，另有 12 和 13 版本可用，但 12 和 13 不支持自动向量化。具体的比较可以参考 `RevyOS` 的[这篇文章](https://revyos.github.io/docs/build/debian/%E7%BC%96%E8%AF%91%E5%99%A8%E7%9B%B8%E5%85%B3%E8%AF%B4%E6%98%8E/)。同时，本题也是下一题的重要前提条件。

#### SG2042 结构

见附件的`SG2042.pdf`，值得注意的是它的`四芯片组成 Cluster`设计。
