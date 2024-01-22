# 流量席卷土豆

## 背景

AD$^{[1]}$ 4202 年，在 Planet Potato 上，运行着 Potato Network。来自 Potato Kingdom University 的师生在 Potato Network 上联网，进行各项社交和研究工作。

> 注：Potato Network 又名“土豆”网，能够以近乎无限的带宽和极低的延迟进行数据传输。唯一的缺陷似乎就是在凌晨时常出现稳定性问题，这似乎是由于它由一堆土豆电池供电。在 AD 4202 年，除了使用可控核聚变发电外，最常见的发电方法就是大规模的生物电池了。为了使得远古系统（例如运行在 Macrohard 2000 上的 tree.pku 服务）能够得以正常运行，Potato Network 在接口上与 Ethernet 和 TCP/IP 保持向下兼容。
>
> Potato Network 在某一个抽象层次是基于“数据包”的，设备接入 Potato Network 产生数据包的同时，可以把所有产生的数据包记录下来，并使用一种名叫 Potato Capture for All Packets (PCAP$^{[2]}$) 的格式存储。

可怜的小土豆是新入职 Potato Kingdom University 的一名工程师。他的职责是运营和维护一个来自远古的庞然大物——他只知道过去人们把它称作“超级计算机”。这该死的大家伙早就该被随处可见的量子比特计算机替代了，只可惜有一些祖传的、使用一种叫做“方程式翻译”的古老语言写就的关键程式，已经没有人能读懂并修改了。维护这大家伙原本是个悠闲差事——古人制造的机器在这千百年来居然鲜有故障，但谁知在交接工作时前任工程师竟然没有告诉他这台机器的根账户密码。

小土豆可以通过自己的账户名和密码登入这台机器，但是如果没有根账户密码，他就不能及时阻止一些问题程序的运行。他刚想求助前同事，却忽然想起了一件事：在这台庞然大物的“共享硬碟”中，留存有前几个月这台计算机接入 Potato Network 产生的所有数据包（packet）。这里面一定包括了前任管理员通过 Potato Network 登入超级计算机所产生的数据包！在 AD 4202，$P=NP$ 已被证明，从数据包中截获并破译密码就是运行个程序的事儿！

## 任务

由于时空管理局的错误操作，误将 AD 2024 年的 Peking University 的超算与 AD 4202 年的 Potato Kingdom University 的超级计算机相连接。你惊奇地发现：你可以从本次比赛提供的超算集群（可以通过 SCOW 平台访问）直接访问 Potato Kingdom University 的超级计算机！好吧，你意识到或许在宇宙的哪个角落哈希算法被某个不负责任的家伙做成了简单的取缩写。你的朋友，小 X，在早些时候已经对这一集群开展了检查，他发现了几个不同寻常的地方，其中引发你的注意的是如下几点：

1. 在 `/usr/bin` 下有一个可疑的可执行文件，名叫 `quantum-cracker`。似乎在集群的所有节点上都可以直接运行它。经过简单的逆向工程，小 X 发现：它似乎能接受一个全部为 SSH 流量的 PCAP 文件作为参数，并从中使用 Potato Kingdom Quantum Engine (Revision Infinite) 破解出密码；
2. 在 `/lustre/shared_data/potato_kingdom_univ_trad_cluster` 中，有一个可疑的文件夹：`pcaps`，里面保存了若干 PCAP 文件：`0~15.pcap`。经过对日志的检查，小 X 发现，这些 PCAP 文件是对 Potato Network 数据包的记录，并且进行了分块。
3. 小 X 很喜欢吃脆脆鲨，他发现集群的所有节点上都安装了一个奇怪的软件包：`wireshark-cli`，<del>长得很像被一根导线捆起来的一坨脆脆鲨</del>。这个软件包提供了一些命令，其中包括 `tshark` 和 `mergecap`。前者似乎可以用于从 PCAP 文件中过滤信息，后者似乎可以用于合并多个 PCAP 文件。

你看着可怜的小土豆面对着一堆来自远古的文档愁眉苦脸，就准备帮他一把，提取出密码。小 X 对你邪魅一笑，并告诉你他已经学会了如何通过量子纠缠向 Potato Kingdom Quantum Post Office 传递信息。你只需获知最终密码，并告诉小 X 即可。

小 X 为了让你学习超算知识，要求你必须使用 4 个计算节点，每个节点 4 个 `tshark` 进程同时从 16 个 PCAP 文件中提取 SSH 流量。为此，他要求你使用集群上的 Slurm 调度器运行这一步骤，同时把进行这一计算的 Slurm JobID 和最终使用 `quantum-cracker` 获得的密码同时交付给他。他会检查这一 JobID 对应的 Slurm Job 是否使用了 4 个计算节点，每个节点 4 个进程。否则他不会动用他的量子纠缠能力。

## 提示

你可以使用集群的 `C064M0256G` 分区进行这一任务。Slurm 提供了一个 `srun` 命令，用于调度并运行程序。你可能会用到它的三个参数：

| 参数                                | 描述                                               |
| ----------------------------------- | -------------------------------------------------- |
| `-p <PARTIION>`                     | `<PARTITION>` 可用于设置运行分区。                 |
| `-N <NODE_NUM>`                     | `<NODE_NUM>` 可用于设置运行所用节点数。            |
| `--ntasks-per-node <TASK_PER_NODE>` | `<TASK_PER_NODE>` 可用于设置每个节点运行的任务数。 |

被 Slurm 执行的程序可以读取 `SLURM_PROCID` 环境变量，获得当前的进程 ID。一个实例：

```
$ srun -N4 --ntasks-per-node=4 bash -c 'echo $SLURM_PROCID' | sort -n
0
1
2
3
4
5
6
7
8
9
10
11
12
13
14
15
```

你可以在你的家目录中临时保存结果。`/lustre` 目录是共享文件夹，在集群所有节点上同步。你的家目录也在 `/lustre` 目录下。

若要获取上一次运行的 Slurm Job 的 JobID，你可以使用 `sacct` 命令。例如，运行 `sacct -u $(whoami) --format JobID | tail`，你会获得类似于如下文本的输出：

```
180.0
181
181.extern
181.0
182
182.extern
182.0
183
183.extern
183.0
```

其中“183”就是我们要寻找的 JobID。依此类推。

若要从 `/lustre/shared_data/potato_kingdom_univ_trad_cluster/0.pcap` 中提取 SSH 流量，并保存到当前目录下的 `0.ssh.pcap`，你可以运行：`tshark -r /lustre/shared_data/potato_kingdom_univ_trad_cluster/0.pcap -Y ssh -w 0.ssh.pcap`。

若要将多个 PCAP 文件（当前目录下的 `*.ssh.pcap`）合并为一个 PCAP 文件（`merged.pcap`），你可以运行：`mergecap -w merged.pcap *.ssh.pcap`。这个 `merged.pcap` 可以被 `quantum-cracker` 处理。

`quantum-cracker`会直接输出破解后的密码。

## 提交方式

你需要提交两个字符串。其中第一个是 Slurm JobID，第二个是破解得到的密码。注意：每个选手获得的密码不一定相同，请不要直接提交别的选手的答案，否则我们将会按照作弊处理。

本题设置部分分，正确的 Slurm JobID 和正确的密码分别占总分的 40% 和 60%。

## 备注

$[1]$: AD，公元。<br>$[2]$: PCAP 文件格式事实上是一种在现实中存在的格式，常被用于记录网络流量和抓包数据。你可以尝试在计算机上安装一个名为 [Wireshark](https://www.wireshark.org/) 的软件来创建和操作这一格式的文件。本题中我们使用的 `tshark` 和 `mergecap` 工具正是 Wireshark 的一部分；使用并行技术分析大规模的网络流量也是当今一个非常常见的实践。
