### introduction

logistic方程
$\frac{dx}{dt}=rx(1-x)$
描述种群在资源有限的情况下数量随时间的变化，logistic映射是与其相似的离散映射，具有如下形式。  
$x=rx(1-x)$

logistic映射尽管形式简单，但作为混沌系统具有很多典型的特征，例如分叉和吸引子。  
当1<r<3时，x会稳定在0-1区间内的某个值。一旦r大于3，会在两个数值中循环形成稳定的周期，随着r的增加，周期长度逐渐翻倍，直到在临界点(r~3.56995)趋于无穷，意味着在这里没有任何的周期现象。  
如果统计r取不同值时x的分布，就会得到如下图案。  
![img](./attachments/untitled.png)

在r大于临界点的若干区间中，仍存在稳定的周期，并且有和临界点左侧相似的逐步分叉现象。这也是logistic映射有趣的地方。

这里将使用logistic映射作为样例，初步了解向量化指令是如何加速数值计算的。

### what should you do

你需要计算n个64位浮点数的itn次logistic映射，所有浮点数会使用相同参数r。n为1024的整数倍。

从输入文件conf.data中依次读取如下内容  
64位整数 itn  
64位浮点 r  
64位整数 n  
初始值，n个64位浮点。  

将迭代itn次后的结果直接写入输出文件out.data中。  

提供样例程序以供参考，由于混沌系统的特性，任何微小的扰动都会导致最终结果的彻底变化，你必须保持样例程序中的运算顺序，先计算r和x的乘积再计算rx和1-x的乘积，也不能将x(1-x)转化为(x*x-x)然后乘以-r。

### example

~~~
#include <iostream>
#include <chrono>

double it(double r, double x, int64_t itn) {
    for (int64_t i = 0; i < itn; i++) {
        x = r * x * (1.0 - x);
    }
    return x;
}

void itv(double r, double* x, int64_t n, int64_t itn) {
    for (int64_t i = 0; i < n; i++) {
        x[i] = it(r, x[i], itn);
    }
}

int main(){
    FILE* fi;
    fi = fopen("conf.data", "rb");

    int64_t itn;
    double r;
    int64_t n;
    double* x;

    fread(&itn, 1, 8, fi);
    fread(&r, 1, 8, fi);
    fread(&n, 1, 8, fi);
    x = (double*)malloc(n * 8);
    fread(x, 1, n * 8, fi);
    fclose(fi);


    auto t1 = std::chrono::steady_clock::now();
    itv(r, x, n, itn);
    auto t2 = std::chrono::steady_clock::now();
    int d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    printf("%d\n", d1);

    fi = fopen("out.data", "wb");
    fwrite(x, 1, n * 8, fi);
    fclose(fi);

    return 0;
}
~~~

### benchmark details

提交单个cpp文件，提交的代码会以最简单的方式编译并执行，编译选项为-O3 -fopenmp -mavx512f，通常编译器会尝试avx512向量化，请注意对齐。如果需要链接特定系统或编译器自带库，请联系工作人员并说明理由，编译条件的更改会对所有选手可见。程序运行在Intel机器中的8个核心上。  
各测试点如下所示，在限制时间内得到正确结果可以得到基本分，在满分时间内得到正确结果可以得到满分，分数随时间的对数线性变化。时间限制小于样例程序用时，意味着提交样例程序无法获得分数。

|序号|n|itn|样例程序用时|时间限制|满分时间|基本分值|满分分值|
|----|-|---|-----------|--------|--------|-------|-------|
|1|4M|32768|6min|60s|1.5s|10|50|
|2|8M|32768|12min|120s|3s|10|50|
