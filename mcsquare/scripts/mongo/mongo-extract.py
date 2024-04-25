import re
import sys
import os

wall_time = {}
def extract_time(file_path, expt, iter):
    if not os.path.isfile(file_path):
        return

    with open(file_path, 'r') as file:
        data = file.readlines()

    for line in data:
        # Regex to match your data format
        match = re.match(r"\[INSERT\], AverageLatency\(us\), ([0-9]+\.[0-9]+)", line.strip())
        if match:
            latency = float(match.group(1))
            if expt not in wall_time:
                wall_time[expt] = []  # Initialize empty list for size if not present
            if(latency > 13000 and latency < 20000):
                wall_time[expt].append(latency)

stalls = {}
def extract_stalls(file_path, expt, iter):
    if not os.path.isfile(file_path):
        return

    with open(file_path, 'r') as file:
        data = file.readlines()
    
    count = 0
    for line in data:
        # Regex to match your data format
        match = re.match(r"system.mem_ctrls[0-9].mcsquare.memElideBlockedCTTFull[\s]+([0-9]+)([\s]+)", line.strip())
        if match:
            stall = int(match.group(1))
            if expt not in stalls:
                stalls[expt] = []  # Initialize empty list for size if not present
            stalls[expt].append(stall)
            count += 1
            if(count == 2):
                break

def main():
    expts = sys.argv[1].split()
    labels = sys.argv[2].split()
    iters = sys.argv[3].split()
    for i in iters:
        for expt in expts:
            extract_time("results/mongo-new/" + expt + "-" + i + "/system.pc.com_1.device", expt, i)
            extract_stalls("results/mongo-new/" + expt + "-" + i + "/stats.txt", expt, i)

    avg_latency = []
    print("Insertion latency (ms)")
    for index in range(len(expts)):
        if(expts[index] not in wall_time):
            print("OUTPUT OF " + expts[index] + " NOT FOUND. RERUN MONGO!")
            continue
        if(len(wall_time[expts[index]]) < 3):
            print("\n\n" + expts[index] + " HAD AN ERROR. TO RERUN EXPERIMENT EXECUTE: make launch_mongo_{:s}\n\n".format(expts[index].split("/")[0]))
            #return
        print(labels[index], end="\t")
        for i in wall_time[expts[index]]:
            print("%.2f" % (i), end="\t")
        if(len(wall_time[expts[index]])):
            avg_latency.append(sum(wall_time[expts[index]][:]) / (len(wall_time[expts[index]]) * 1000))
            print("Avg %.2f" % (avg_latency[-1]), end="\t")
        print()
    
    import matplotlib.pyplot as plt
    import numpy as np
    plt.figure(figsize=(8, 4))
    plt.barh(labels, avg_latency, color=['darkblue', 'purple', 'orange'])
    plt.xlabel('Average latency (ms)')  # Label the x-axis
    plt.xticks(np.arange(0, max(avg_latency) + 5, step=5))
    plt.savefig(sys.argv[4])  # Save the chart to a file

if __name__ == "__main__":
    main()