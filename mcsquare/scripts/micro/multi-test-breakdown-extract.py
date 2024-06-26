import re
import sys
import matplotlib.pyplot as plt
import numpy as np

def extract_cycles(file_path):
    with open(file_path, 'r') as file:
        data = file.readlines()

    # Dictionary to hold experiment cycles
    experiment_cycles = []
    experiment_number = 0
    max_cycles = 0

    for line in data:
        # Regex to match your data format
        match = re.match(r"system\.switch_cpus(\d+)\.numCycles\s+(\d+)", line.strip())
        if match:
            cpu_num, cycles = int(match.group(1)), int(match.group(2))
            #print(match, cpu_num, cycles)
            
            if cpu_num == 0 and experiment_number > 0: # Start of a new experiment
                experiment_cycles.append(max_cycles)
                max_cycles = 0
                
            max_cycles = max(max_cycles, cycles)
            experiment_number += 1
    
    # Append the max cycles for the last experiment
    experiment_cycles.append(max_cycles)

    return experiment_cycles

def extract_ticks(file_path):
    with open(file_path, 'r') as file:
        data = file.readlines()

    # Dictionary to hold experiment cycles
    experiment_ticks = []

    for line in data:
        # Regex to match your data format
        match = re.match(r"simTicks\s+(\d+)", line.strip())
        if match:
            experiment_ticks.append(int(match.group(1)))

    return experiment_ticks

sizes=["64B", "256B", "1KB", "4KB", "16KB", "64KB", "256KB", "1MB", "4MB"]
expts=["Cacheline writeback", "Packet to memctrl"]
def main():
    file_path = sys.argv[1]
    '''
    experiment_cycles = extract_cycles(file_path)
    print("Max CPU cycles")
    print("size", end="\t")
    for expt in expts:
        print("%s" % expt, end="\t"),
    print(""),

    i = 0
    for size in sizes:
        print("%s" % size, end="\t"),
        for expt in expts:
            print("%d" % experiment_cycles[i], end="\t"),
            i += 1
        print()
    print()
    '''
    experiment_ticks = extract_ticks(file_path)
    print("Overhead (%)")
    print("Copy size", end="\t")
    for expt in expts:
        print("%s" % expt, end="\t"),
    print(""),

    i = 0
    expt_results = {}
    for size in sizes:
        print("%s" % size, end="\t"),
        total = float(sum(experiment_ticks[i:i+len(expts)]))
        for expt in expts:
            print("%.2f" % (100.0 * float(experiment_ticks[i]) / total), end="\t"),
            if expt not in expt_results:
                expt_results[expt] = []
            expt_results[expt].append(100.0 * float(experiment_ticks[i]) / total)
            i += 1
        print()

    plt.figure(figsize=(8, 4))
    for expt in expts:
        plt.plot(sizes, expt_results[expt], '.-', label=expt)  # Plot the chart
    plt.xlabel('Copy size')  # Label the x-axis
    plt.ylabel('Overhead contribution (%)')  # Label the x-axis
    plt.legend()
    plt.savefig(sys.argv[2])  # Save the chart to a file

if __name__ == "__main__":
    main()