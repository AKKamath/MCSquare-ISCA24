import re
import sys

def extract_cycles(file_path, test, cycle_list):
    with open(file_path, 'r') as file:
        data = file.readlines()

    # Dictionary to hold experiment cycles
    experiment_cycles = [test]

    for line in data:
        # Regex to match your data format
        match = re.match(r"Iter (\d+): \t(\d+) cycles", line.strip())
        if match:
            iter, cycles = int(match.group(1)), int(match.group(2))
            experiment_cycles.append(cycles)
    
    # Append the max cycles for the last experiment
    cycle_list.append(experiment_cycles)

def main():
    cycle_list = []
    tests = sys.argv[1]
    for test in tests.split():
        extract_cycles("results/kernel/" + test + "/system.pc.com_1.device", test, cycle_list)

    #print(cycle_list)
    num_tests = len(tests.split())

    for j in range(num_tests):
        for i in range(len(cycle_list[j])):
            print("%s" % (cycle_list[j][i]), end="\t")
        print("")


if __name__ == "__main__":
    main()