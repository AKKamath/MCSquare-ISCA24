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

sizes=[64, 256, "1KB", "4KB", "16KB", "64KB", "256KB", "1MB", "4MB"]
zio_ignore = [64, 256, "1KB", "4KB"]
file1=["(MC)^2", "Memcpy", "zIO"]
file2="Touched memcpy"
expts=["Memcpy", "Touched memcpy", "zIO", "(MC)^2"]
def main():
    file_paths = sys.argv[1].split()
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
    # Extract data from files and place into dict
    experiment_ticks_file1 = extract_ticks("results/micro/" + file_paths[0] + "/stats.txt")
    experiment_ticks_file2 = extract_ticks("results/micro/" + file_paths[1] + "/stats.txt")

    experiment_ticks = {}
    for expt in file1 + [file2]:
        experiment_ticks[expt] = []
    
    i = 0
    for size in sizes:
        for expt in file1:
            experiment_ticks[expt].append(experiment_ticks_file1[i])
            i += 1
    i = 0
    for size in sizes:
        experiment_ticks[file2].append(experiment_ticks_file2[i])
        i += 1
       
    print("Total ticks")
    print("size", end="\t")
    for expt in expts:
        print("%s" % expt, end="\t"),
    print(""),

    i = 0
    for size in sizes:
        print("%s" % size, end="\t"),
        for expt in expts:
            if(expt == "zIO" and size in zio_ignore):
                # zIO is only active for 16KB and above
                # Can verify this by seeing if "fast copies: 1" is outputted by zIO
                print("", end="\t"),
            elif expt == "Touched memcpy" and size == "4MB":
                # 4MB exceeds cache size and is not representative of cached performance
                print("", end="\t"),
            else:
                print("%d" % experiment_ticks[expt][i], end="\t"),
        print()
        i += 1
    plt.figure(figsize=(8, 4))
    for expt in expts:
        if(expt == "zIO"):
            # zIO is only active for 16KB and above
            # Can verify this by seeing if "fast copies: 1" is outputted by zIO
            plt.plot(sizes[4:], experiment_ticks[expt][4:], '.-', label=expt)
        elif expt == "Touched memcpy":
            # 4MB exceeds cache size and is not representative of cached performance
            plt.plot(sizes[:-2], experiment_ticks[expt][:len(sizes)-2], '.-', label=expt)
        else:
            plt.plot(sizes, experiment_ticks[expt][:len(sizes)], '.-', label=expt)
    plt.xlabel('Copy size')  # Label the x-axis
    plt.ylabel('Copy latency (ns)')  # Label the x-axis
    plt.yscale('log')
    plt.legend()
    plt.savefig(sys.argv[2])  # Save the chart to a file

if __name__ == "__main__":
    main()