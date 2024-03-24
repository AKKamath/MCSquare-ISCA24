import re
import sys

def extract_cycles(file_path):
    with open(file_path, 'r') as file:
        data = file.readlines()

    # Dictionary to hold experiment cycles
    exp_cycles = {}
    reads = {}
    accs = {}

    for line in data:
        # Regex to match your data format
        match = re.match(r"0 threads, (\d+) size; (\d+\.\d+): Read pipe: (\d+) cycles. Read (\d+), acc (\d+);", line.strip())
        if match:
            size, cycles, read_time, acc_time = int(match.group(1)), int(match.group(3)), int(match.group(4)), int(match.group(5))
            if size not in exp_cycles:
                exp_cycles[size] = []  # Initialize empty list for size if not present
                reads[size] = []
                accs[size] = []
            exp_cycles[size].append(cycles)
            reads[size].append(read_time)
            accs[size].append(acc_time)
    # Append the max cycles for the last experiment
    return exp_cycles, reads, accs

def main():
    cycle_list = []
    tests = sys.argv[1].split()
    labels = sys.argv[2].split()
    sizes = sys.argv[3].split()

    throughput_vals = {}
    print("Size", end="\t")
    for index in range(len(tests)):
        print(f"{labels[index]}", end="\t")
        throughput_vals[tests[index]] = []
    print()
    for size in sizes:
        print(f"{size}B", end="\t")
        for test in tests:
            cycle_list, reads, accs = extract_cycles("results/kernel/pipe/" + test + "-" + size + "/system.pc.com_1.device")
            throughput=-1
            for i in cycle_list.keys():
                for j in range(len(cycle_list[i])):
                    throughput=i / (cycle_list[i][j] / 1000) * 200
                throughput /= len(cycle_list[i])
            throughput_vals[test].append(throughput)
            print("{:.2f}".format(throughput), end="\t")
        print("")
    import matplotlib.pyplot as plt
    import numpy as np
    plt.figure(figsize=(8, 4))
    x = np.arange(5)
    width=0.4
    for test in tests:
        plt.bar(x, throughput_vals[test], width)
        x = [i + width for i in x]
    x = [i - width * float(len(tests)) / 2 for i in x]
    plt.xticks(x, [x + "B" for x in sizes])
    plt.xlabel('Transfer size')  # Label the x-axis
    plt.yticks(np.arange(0, 501, 100))
    plt.ylim(0, 500)
    plt.ylabel('Throughput (Bytes/kCycle)')
    plt.legend(labels)
    plt.savefig(sys.argv[4])  # Save the chart to a file

if __name__ == "__main__":
    main()