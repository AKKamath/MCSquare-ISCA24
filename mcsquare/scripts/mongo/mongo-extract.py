import re
import sys
import os

wall_time = {}
def extract_time(file_path, expt, iter):
    if not os.path.isfile(file_path):
        return

    with open(file_path, 'r') as file:
        data = file.readlines()

    for line in data:
        # Regex to match your data format
        match = re.match(r"\[INSERT\], AverageLatency\(us\), ([0-9]+\.[0-9]+)", line.strip())
        if match:
            latency = float(match.group(1))
            if expt not in wall_time:
                wall_time[expt] = []  # Initialize empty list for size if not present
            wall_time[expt].append(latency)

stalls = {}
def extract_stalls(file_path, expt, iter):
    if not os.path.isfile(file_path):
        return

    with open(file_path, 'r') as file:
        data = file.readlines()
    
    count = 0
    for line in data:
        # Regex to match your data format
        match = re.match(r"system.mem_ctrls[0-9].mcsquare.memElideBlockedCTTFull[\s]+([0-9]+)([\s]+)", line.strip())
        if match:
            stall = int(match.group(1))
            if expt not in stalls:
                stalls[expt] = []  # Initialize empty list for size if not present
            stalls[expt].append(stall)
            count += 1
            if(count == 2):
                break

def main():
    expts = sys.argv[1]
    iters = sys.argv[2]
    for i in iters.split():
        for expt in expts.split():
            extract_time("results/mongo-new/" + expt + "-" + i + "/system.pc.com_1.device", expt, i)
            extract_stalls("results/mongo-new/" + expt + "-" + i + "/stats.txt", expt, i)

    print("Insertion latency")
    for expt in expts.split():
        if(expt not in wall_time):
            print(expt, "not found")
            continue
        print(expt, end="\t")
        for val in wall_time[expt]:
            print("%.2f" % (val), end="\t")
        print()
    print()

    print("Stalls")
    for expt in expts.split():
        if(expt not in stalls):
            print(expt, "not found")
            continue
        print(expt, end="\t")
        for val in stalls[expt]:
            print("%.2f" % (val), end="\t")
        print()

if __name__ == "__main__":
    main()