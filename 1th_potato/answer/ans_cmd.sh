srun -N4 --ntasks-per-node=4 bash -c 'srun -N4 -n16 tshark -r $SLURM_PROCID.pcap -Y "ssh" -w output/$SLURM_PROCID.ssh.pcap'
mergecap -w merged.pcap *.ssh.pcap
quantum-cracker merged.pcap
