# H. 矩阵乘法

## 题面

### 背景介绍

矩阵乘法经常出现在实际代数运算中，被各种计算库（如 Eigen，MKL，OpenBLAS）集成，也经常用于基准测试中。在尺度较小时，数据量近似等于计算量；如果尺度翻倍，计算量变为原先的 8 倍，但数据量仅为原先的 4 倍；矩阵乘法存在随尺度增大的数据局域性。由于其独特的计算量和数据量增长关系，一个优秀的矩阵相乘算法必须充分利用各级存储资源。

### 任务描述

你需要从文件 `conf.data` 中读取两个矩阵 $M_1$，$M_2$ 并计算乘积 $M_3=M_1 \cdot M_2$，然后将结果写入文件 `out.data` 中。

输入文件为一个二进制文件，定义如下：

1. 三个 64 位整数 $N_1$, $N_2$, $N_3$；
2. 第一个矩阵 $M_1$ 的数据，矩阵有 $N_1$ 行、$N_2$ 列，以 64 位浮点的形式存储，顺序为行优先；
3. 第一个矩阵 $M_2$ 的数据，矩阵有 $N_2$ 行、$N_3$ 列，以 64 位浮点的形式存储，顺序为行优先。

将矩阵 $M_3$ 直接写入输出文件中，以 64 位浮点的形式存储，顺序为行优先。

输入输出文件均在内存盘上。我们提供了一组样例测试数据，可以从 `附件` 选项卡中下载。

### 样例程序

```cpp
#include <iostream>
#include <chrono>

void mul(double* a, double* b, double* c, uint64_t n1, uint64_t n2, uint64_t n3) {
 for (int i = 0; i < n1; i++) {
  for (int j = 0; j < n2; j++) {
   for (int k = 0; k < n3; k++) {
    c[i * n3 + k] += a[i * n2 + j] * b[j * n3 + k];
   }
  }
 }
}

int main() {
 uint64_t n1, n2, n3;
 FILE* fi;

 fi = fopen("conf.data", "rb");
 fread(&n1, 1, 8, fi);
 fread(&n2, 1, 8, fi);
 fread(&n3, 1, 8, fi);

 double* a = (double*)malloc(n1 * n2 * 8);
 double* b = (double*)malloc(n2 * n3 * 8);
 double* c = (double*)malloc(n1 * n3 * 8);

 fread(a, 1, n1 * n2 * 8, fi);
 fread(b, 1, n2 * n3 * 8, fi);
 fclose(fi);

 for (uint64_t i = 0; i < n1; i++) {
  for (uint64_t k = 0; k < n3; k++) {
   c[i * n3 + k] = 0;
  }
 }

 auto t1 = std::chrono::steady_clock::now();
 mul(a, b, c, n1, n2, n3);
 auto t2 = std::chrono::steady_clock::now();
 int d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
 printf("%d\n", d1);


 fi = fopen("out.data", "wb");
 fwrite(c, 1, n1 * n3 * 8, fi);
 fclose(fi);

 return 0;
}
```

### 评测方式

提交单个 `.cpp` 文件，提交的代码会以最简单的方式编译并执行，编译选项为 `-O3 -fopenmp -mavx512f`，通常编译器会尝试 `avx512` 向量化，也可以使用 avx512 内置函数。如果需要链接特定系统或编译器自带库，请联系工作人员并说明理由，编译条件的更改会对所有选手可见。程序运行在 Intel Xeon 8358 机器中的 8 个核心上。  
各测试点如下所示，在限制时间内得到正确结果可以得到基本分，在满分时间内得到正确结果可以得到满分，分数随时间的对数线性变化。除第一个测试点外，时间限制小于样例程序用时，意味着提交样例程序无法获得分数。

| 序号 | n1,n2,n3           | 样例程序用时 | 时间限制 | 满分时间 | 基本分值 | 满分分值 |
| ---- | ------------------ | ------------ | -------- | -------- | -------- | -------- |
| 1    | 1024，1024，1024   | 0.4s        | 1s       | 0.1s     | 2        | 2        |
| 2    | 8192，8192，8192   | 6min         | 50s      | 2.6s     | 2        | 18       |
| 3    | 8192，16384，8192  | 12min         | 100s      | 5.0s     | 3        | 40       |
| 4    | 16384，16384，8192 | 24min         | 200s     | 9.6s     | 3        | 40       |
