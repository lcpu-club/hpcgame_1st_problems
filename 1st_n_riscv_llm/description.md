# N. RISC-V LLM

## 题面

### 题目说明

请先阅读RISC-V OpenBLAS的题目说明。建议在有可以正确执行的OpenBLAS的前提下完成本题。

LLM自出现就成为了日益重要的工具，也成为了软硬件体系性能的重要测试场景。本题要求选手运行4bit量化过的`ChatGLM2` ，进行性能测试。

本题在总分中占比为0分，在RISC-V赛道中的权重为`50%`。

### 框架

我们建议使用如下框架。由于本题是手工评测，选手也可以提交任意框架，但必须有详细的编译方法和使用方法，以及相应权重。权重必须至少是`q4_0`量化的，即允许更高精度的权重，但不再允许向更低精度量化。

- [chatglm.cpp](https://github.com/li-plus/chatglm.cpp)

### 提交方式

选手提交一个`tar.xf`文件，使用`tar xf --strip-components=1 file.tar.gz`解压后，根目录需要有一个`CMakeLists.txt`文件。评测机将会使用`cmake -B build && cmake --build build -j 32`进行编译。
并使用如下代码评测。该代码将使用不同prompt执行十次，所得结果取平均值。

```bash
./build/bin/main -m /opt/models/chatglm2-6b-int4.bin --mode generate -p "当前评测prompt" --max_new_tokens 100 --temp 0 -t 64
```

### 性能评分

最终性能评分为`prompt tokens` + `1.5 * output tokens`之和除以生成时间。读取权重的耗时将被减去。每秒0.5个`token`以下为零分，每秒3个`token`为满分，即$本题得分=min(100, (throughput-0.5)/1.5*100)$。

### 赛道奖励说明

RISC-V赛道共两道题目，奖金总额两万元。由有有效成绩的选手中的前20位进行奖金分配。若一名选手的成绩为$x$，在奖金分配中的权重为$x^2$。如选手奖金低于200元，将采用自选奖品的形式发放。
