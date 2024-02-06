# 答案

执行下列命令即可。

```bash
srun -N4 --ntasks-per-node=4 -n16 bash -c 'tshark -r /path/to/pcaps/$SLURM_PROCID.pcap -Y "ssh" -w output/$SLURM_PROCID.ssh.pcap'
mergecap -w merged.pcap *.ssh.pcap
quantum-cracker merged.pcap
```

`merged.pcap` 的哈希值为 `83637877eac153e236112f98bebae50b905c62685c519e5b2cb57ab8bfda31cb`。

如果遇到超出内存限制的情况，提供一种可能的解决思路：

多开几个 Task（例如开 32 个），但是提交 Task ID 的时候，提交一个自己别的 4 Node 16 Task 的 Job 的 Task ID，这样就可以绕过内存限制了。

但是在处理我们提供的 pcap 文件时理论上不会超过内存限制。
