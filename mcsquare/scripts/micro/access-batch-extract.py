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

sizes=["0%", "12.5%", "25%", "50%", "100%"]
def main():
    file_path = sys.argv[1]
    configs = sys.argv[2]

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

    print("Total ticks")
    print("", end="\t")
    for size in sizes:
        print("%s" % size, end="\t"),
    print(""),
    for config in configs.split():
        file_name = file_path + config + "/stats.txt"
        experiment_ticks = extract_ticks(file_name)

        i = 0
        print("%s" % config, end="\t")
        for size in experiment_ticks:
            print("%d" % size, end="\t")
            i += 1
            if i % len(sizes) == 0 and len(experiment_ticks) > i:
                if i == 2 * len(sizes):
                    break
                print()
                print("%s-align" % config, end="\t")
        print()

if __name__ == "__main__":
    main()