import re
import sys

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
        else:
            match = re.match(r"system\.switch_cpus\.numCycles\s+(\d+)", line.strip())
            if match:
                max_cycles = int(match.group(1))
                experiment_cycles.append(max_cycles)
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

sizes=[0, 12.5, 25, 50, 100]
def main():
    file_path = sys.argv[1]
    configs = sys.argv[2]
    '''
    print("Max CPU cycles")
    print("", end="\t")
    for size in sizes:
        print("%s" % size, end="\t"),
    print("")

    for config in configs.split():
        file_name = file_path + config + "/stats.txt"
        experiment_cycles = extract_cycles(file_name)

        i = 0
        print("%s" % config, end="\t")
        for size in experiment_cycles:
            print("%d" % size, end="\t")
            i += 1
        print()
    '''

    experiment_ticks = {}
    for config in configs.split():
        file_name = file_path + config + "/stats.txt"
        ticks_indiv = extract_ticks(file_name)
        if config == "MCSquare":
            experiment_ticks[config] = ticks_indiv[:len(ticks_indiv) // 2]
            experiment_ticks[config + " [Aligned]"] = ticks_indiv[len(ticks_indiv) // 2:]
        else:
            experiment_ticks[config] = ticks_indiv

    # Normalize performance
    norm_perf = {}
    for config in experiment_ticks.keys():
        norm_perf[config] = []
    
    i = 0
    for size in sizes:
        for config in experiment_ticks.keys():
            norm_perf[config].append(float(experiment_ticks[config][i]) / float(experiment_ticks["Memcpy"][i]))
        i += 1

    print("Normalized performance")
    print("Access size", end="\t")
    for config in experiment_ticks.keys():
        print("%s" % config, end="\t"),
    print(""),

    i = 0
    for size in sizes:
        print("%s%%" % size, end="\t")
        for config in norm_perf.keys():
            print("%.3f" % (norm_perf[config][i]), end="\t")
        print()
        i += 1

    import matplotlib.pyplot as plt
    import numpy as np
    plt.figure(figsize=(8, 4))
    for config in experiment_ticks.keys():
        plt.plot(sizes, norm_perf[config], '.-', label=config)  # Plot the chart
    plt.xticks(np.arange(0, 101, 25))
    plt.yticks(np.arange(0, 2.1, 0.5))
    plt.xlabel('Portion of dataset accessed (%)')  # Label the x-axis
    plt.ylabel('Normalized runtime')  # Label the x-axis
    plt.legend()
    plt.savefig(sys.argv[3])  # Save the chart to a file

if __name__ == "__main__":
    main()