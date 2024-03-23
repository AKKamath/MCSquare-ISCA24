import re
import sys

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

sizes=["16KB", "64KB", "256KB", "1MB", "4MB"]
def main():
    file_path = sys.argv[1]
    configs = sys.argv[2].split()
    expt_ticks = {}
    for config in configs:
        file_name = file_path + config + "/stats.txt"
        expt_ticks[config] = extract_ticks(file_name)

    # Normalize performance
    norm_perf = {}
    for config in expt_ticks.keys():
        norm_perf[config] = []
    
    i = 0
    for size in sizes:
        for config in expt_ticks.keys():
            norm_perf[config].append(float(expt_ticks[config][i]) / float(expt_ticks["1"][i]))
        i += 1

    print("Normalized runtime")
    print("Size", end="\t")
    for config in expt_ticks.keys():
        print("%s" % config, end="\t"),
    print(""),

    i = 0
    for size in sizes:
        print("%s" % size, end="\t")
        for config in norm_perf.keys():
            print("%.3f" % (norm_perf[config][i]), end="\t")
        print()
        i += 1

    import matplotlib.pyplot as plt
    import numpy as np
    plt.figure(figsize=(8, 4))
    x = np.arange(5)
    width=0.1
    for val in configs:
        plt.bar(x, norm_perf[val], width)
        x = [i + width for i in x]
    x = [i - width * float(len(configs)) / 2 for i in x]
    plt.xticks(x, sizes)
    plt.xlabel('Buffer size')  # Label the x-axis
    plt.ylabel('Normalized runtime')
    plt.legend(configs)
    plt.savefig(sys.argv[3])  # Save the chart to a file
if __name__ == "__main__":
    main()