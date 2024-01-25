import numpy as np
import os

def compare_weights():
    batch_size = 100
    num_batches = 200
    num_workers = 4
    input_prefix = "inputs"
    weight_prefix = "weights"
    hidden_size = 40000
    
    for i in range(batch_size):
        try:
            data = np.load(f"outputs/output_{i}.npy")
        except FileNotFoundError:
            print(f"File 'outputs/output_{i}.npy' not found")
            exit(0)
        std = np.load(f"/lustre/shared_data/ray/820ede54516fbcb6319ca8dc048d1cfe/answers/output_{i}.npy")
        # relative error of 1e-4
        if not np.allclose(std, data, rtol=1e-4):
            print(f"batch {i} not equal")
            exit(0)
        
    print("all equal")
    
if __name__ == "__main__":
    compare_weights()
