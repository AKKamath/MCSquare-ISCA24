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
    
    # Append the max cycles for the last experiment
    experiment_cycles.append(max_cycles)

    return experiment_cycles

expts=["clflush_src", "memcpy", "zIO"]
def main():
    file_path = sys.argv[1]
    experiment_cycles = extract_cycles(file_path)

    print("", end="\t")
    for expt in expts:
        print("%s" % expt, end="\t"),
    print("")

    print("Aligned", end="\t"),
    i = 0
    for expt in expts:
        print("%d" % experiment_cycles[i], end="\t"),
        i += 1
    print()

    print("Unaligned", end="\t"),
    for expt in expts:
        print("%d" % experiment_cycles[i], end="\t"),
        i += 1
    print()


if __name__ == "__main__":
    main()