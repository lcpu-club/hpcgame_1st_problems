#!/usr/bin/env python

import ray
import numpy as np
import time
import os

num_batches = 100
num_workers = 4
input_prefix = "inputs"
output_prefix = "outputs"
weight_prefix = "weights"

@ray.remote(num_cpus=4)
class Worker:

    def __init__(self, weight_path: str, rank: int):
        self.weight = np.load(weight_path)
        self.rank = rank
    
    def calculate(self, x: np.ndarray):
        return np.maximum(x @ self.weight, 0)
    
def main():
    
    start_time = time.time()

    if not os.path.exists(output_prefix):
        os.makedirs(output_prefix)
    
    feature = np.load(f"{input_prefix}/input_0.npy")
    
    workers = []
    features = []
    
    ray.init()
    
    for worker_idx in range(0, num_workers):
        workers.append(Worker.remote(f"{weight_prefix}/weight_{worker_idx}.npy", worker_idx))
    
    for input_idx in range(0, num_batches):
        for worker in workers:
            feature = worker.calculate.remote(feature)
        features.append(feature)
        if input_idx != num_batches - 1:
            feature = np.load(f"{input_prefix}/input_{input_idx + 1}.npy")
    
    for i, result in enumerate(features):
        output = ray.get(result)
        np.save(f"outputs/output_{i}.npy", output)
    
    print(f"Time taken: {time.time() - start_time}")
    
if __name__ == "__main__":
    main()
