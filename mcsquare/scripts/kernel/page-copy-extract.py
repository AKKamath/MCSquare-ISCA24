import re
import sys
import numpy as np

def extract_cycles(file_path, test, cycle_list):
    with open(file_path, 'r') as file:
        data = file.readlines()

    # Dictionary to hold experiment cycles
    experiment_cycles = []

    for line in data:
        # Regex to match your data format
        match = re.match(r"Iter (\d+): \t(\d+) cycles", line.strip())
        if match:
            iter, cycles = int(match.group(1)), float(match.group(2))
            experiment_cycles.append(cycles)
    
    # Append the max cycles for the last experiment
    cycle_list[test] = experiment_cycles

def main():
    cycle_list = {}
    tests = sys.argv[1].split()
    test_names = sys.argv[2].split()
    for test in tests:
        extract_cycles("results/kernel/" + test + "/system.pc.com_1.device", test, cycle_list)

    access_cycles = {}
    for j in tests:
        access_cycles[j] = []
        for i in np.arange(1, len(cycle_list[j]) - 1):
            access_cycles[j].append((cycle_list[j][i+1] - cycle_list[j][i]) / 1000)

    for j in range(len(tests)):
        print("%s" % test_names[j], end="\t")
    print()

    for i in range(len(access_cycles[tests[j]])):
        for j in range(len(tests)):
            print("%.3f" % (access_cycles[tests[j]][i]), end="\t")
        print("")

    import matplotlib.pyplot as plt
    plt.figure(figsize=(8, 4))
    for j in range(len(tests)):
        plt.plot(np.arange(0, len(access_cycles[tests[j]]), 1), access_cycles[tests[j]], '.-', label=test_names[j])  # Plot the chart
    plt.yscale('log')
    plt.xlabel('Accesses')  # Label the x-axis
    plt.ylabel('Kilocycles')  # Label the x-axis
    plt.legend()
    plt.savefig(sys.argv[3])  # Save the chart to a file

if __name__ == "__main__":
    main()