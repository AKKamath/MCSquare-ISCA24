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
        match = re.match(r"(\d+\.\d+): Read pipe: (\d+) cycles. Read (\d+), acc (\d+);", line.strip())
        if match:
            size, cycles, read_time, acc_time = str(match.group(1)), int(match.group(2)), int(match.group(3)), int(match.group(4))
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
    tests = sys.argv[1]
    for test in tests.split():
        cycle_list, reads, accs = extract_cycles("results/kernel/" + test + "/system.pc.com_1.device")
        print(f"{test} - cycles")
        for size, cycles in cycle_list.items():
            print(f"{size}", end="\t")
            for i in cycles:
                print(f"{i}", end="\t")
            print("")
        print(f"{test} - reads")
        for size, cycles in reads.items():
            print(f"{size}", end="\t")
            for i in cycles:
                print(f"{i}", end="\t")
            print("")
        print(f"{test} - accs")
        for size, cycles in accs.items():
            print(f"{size}", end="\t")
            for i in cycles:
                print(f"{i}", end="\t")
            print("")


if __name__ == "__main__":
    main()