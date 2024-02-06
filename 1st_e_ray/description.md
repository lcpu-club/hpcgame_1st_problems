# E. 齐心协力

## 题面

**重要：注：如果遇到 `Cannot Connect to GCS` 或类似报错的请在选手群内私聊管理提交链接重测，这是由 Ray 集群启动异常造成的。如果遇到 `Actor Died` 或类似报错，那是由于超出了内存限制，请自行研究解决，感谢！**

### 题目描述

遥远的开爱大陆盛产毒药，这些毒药威力并不高，只是会让人痛苦不堪，乖乖地给开爱国王交上赎金获取解药。开爱国王富得流油，但人们对此苦不堪言。

你和宿舍小伙伴们是拉玛大陆骑士学院的优秀学员，致力于推翻开爱国王对解药的垄断，于是踏上了寻找解药方法的路程。路上你们遇到了善良的开爱国公主，她告诉你们，每一种毒药都有40000种可能的原料，构成药物空间的一个40000维32位浮点向量。而对应的解药就是将这个向量进行变幻得到的另一种配方。

经过对厚厚几万页配方书的阅读，你们最终发现，从毒药到解药的变换分四步，每一步都是先进行一个线性变换，然后对0取Max（因为原料没有负数个）。这个线性变换类似于神经网络，具体描述如图。其中每个矩阵是的维度是(40000, 40000)。

大家都听说你们掌握了破解毒药的方法，纷纷找你们帮忙。虽然你们可以手算解药变换，不过依然是太慢了。于是你们打算用高性能计算的思想，让电脑来算这件事。但是你们宿舍的电脑都只有16G内存，每一个矩阵就需要12G内存，在一台电脑上放不下，需要四个人齐心协力，共同计算。但是天上不会掉馅饼，怎么才能让四台电脑一起计算呢？你听说Ray最近很热，是基于Python的，编程比MPI更加自然，于是想要试一试。

<center><img src="https://hpcgame.pku.edu.cn/oss/images/public/problems/1th_ray_fcrelu.png" alt="fcrelu" style="zoom:35%;" /></center>

### 任务描述

对于一百个(200, 40000)的矩阵$x$，计算如下结果，其中 $A$、$B$ 、$C$ 和 $D$ 都是 (40000, 40000) 的矩阵。其中四个矩阵需要被放置在四个不同的节点上，每个节点有4个核心、16G内存。你需要对于输入的每一个$x$，计算得出最终的$output$。
$$
y_1 = ReLU(xA) \newline
y_2 = ReLU(y_1B) \newline
y_3 = ReLU(y_2C) \newline
output = ReLU(y_3D) \newline
$$

### 输入输出约定

#### 输入

在`inputs`文件夹中有`input_0.npy`到`input_99.npy`共100个文件，每个文件都是(200, 40000)的`numpy`的`ndarry`，可以通过`np.load`方法读取，如`np.load(f"inputs/input_{i}.npy")`

$A$、$B$ 、$C$ 和$D$矩阵保存在`weights`文件夹下，命名为`weight_[0-4].npy`，如$A$保存在`weight_0.npy`中。

#### 输出

对于inputs中的每个输入，计算完成后将`ndarray`通过`np.save`的方式保存到`outputs`文件夹下，如`np.save(f"outputs/output_0.npy", output)`。

提示：`outputs` 文件夹需要自己建。

#### 误差范围

与标准答案的相对误差在`1e-4`的范围内即即为正确。

#### 说明

输入输出文件夹都使用了超级高速文件系统。不需要太考虑优化读写。

权重文件夹没有使用超级高速文件系统，不建议重复读写权重。

### 环境说明和程序调用方式

提供四个节点，node1-4，每个节点4个核心，16G内存，节点之间有100G高速网络连接。

我们会在node1上通过`python3 main.py`运行你的程序，并在你的程序运行完后比较`output`文件夹中的输出。`module load python/hpcgame`可以在超算集群上使用`python`。注意，环境里没有`pytorch`。

四个节点已经启动了`ray`集群，Head 节点地址可以通过环境变量`RAY_CLUSTER_ADDR`读到。通过以下命令可以连接到集群。

```python
import os
import ray

ray.init(address=f"{os.environ['RAY_CLUSTER_ADDR']}")
```

### Ray的简要介绍

1. 核心介绍：<https://docs.ray.io/en/latest/ray-core/walkthrough.html>，参考 `Ray Core` 部分，主要是Task和Actor的抽象。当然，其他部分也很好，不过本次比赛没有涉及。
2. Placement Groups的概念：<https://docs.ray.io/en/latest/ray-core/scheduling/placement-group.html>，在本题中，使用PACK就可以。

### 提示

可以在四个机器之间维护一个pipeline，最大化硬件利用率。[这篇文章](https://medium.com/nerd-for-tech/an-overview-of-pipeline-parallelism-and-its-research-progress-7934e5e6d5b8)介绍了pipeline parallelism的相关概念，如果你学过ICS或者相似课程的话，应该很好理解。如下图，横轴是每个已经加载好权重的worker，纵轴是随着时间的流逝，每个worker里面处理的batch。可以看到，每个worker权重是确定的，接收前一个worker传递过来的中间数据，处理完再传递给下一个worker或者输出。

<center><img src="https://hpcgame.pku.edu.cn/oss/images/public/problems/1th_ray_pp.png" alt="pp" style="zoom:40%;" /></center>

输入数据已经为你划分好了 `batch`，各阶段时间相同，非常适合 pipeline parallelism。

### 出题人的话

以往大家学高性能计算，都是从 MPI 和 OpenMP 出发的，出题人也不例外。不过近些年，许多具有更高抽象层次的API出现，比起消息传递来说更加符合入门的认知规律。这次给大家介绍的Ray，就是高抽象层次API中的典型代表。

### 得分标准

| 最低得分时间 | 满分时间 | 最低得分 | 满分 |
| --- | --- | --- | --- |
| 400s | 200s | 1% | 100% |

### 测试指南

选手可以在集群自己提交任务，进行测试。具体来说，我们在附件提供了三个sbatch文件和一个shell脚本。将脚本和Python文件，以及你自己准备的input和output放在同一个文件夹下，就能进行测试。
