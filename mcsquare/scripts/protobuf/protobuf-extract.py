import re
import sys

wall_time = {}
cpu_time = {}
def extract_time(file_path, ctt_size = 0, ctt_frac = 0):
    with open(file_path, 'r') as file:
        data = file.readlines()

    for line in data:
        # Regex to match your data format
        match = re.match(r"BM_Protogen_Arena[\s]+([0-9]+) ns[\s]+([0-9]+) ns", line.strip())
        if match:
            wall, cpu = int(match.group(1)), int(match.group(2))
            if ctt_size not in wall_time:
                wall_time[ctt_size] = {}  # Initialize empty list for size if not present
                cpu_time[ctt_size] = {}  # Initialize empty list for size if not present
            wall_time[ctt_size][ctt_frac] = wall
            cpu_time[ctt_size][ctt_frac] = cpu

stalls = {}
def extract_stalls(file_path, ctt_size, ctt_frac):
    with open(file_path, 'r') as file:
        data = file.readlines()
    
    count = 0
    for line in data:
        # Regex to match your data format
        match = re.match(r"system.mem_ctrls[0-9].mcsquare.memElideBlockedCTTFull[\s]+([0-9]+)([\s]+)", line.strip())
        if match:
            stall = int(match.group(1))
            if ctt_size not in stalls:
                stalls[ctt_size] = {}  # Initialize empty list for size if not present
            if ctt_frac not in stalls[ctt_size]:
                stalls[ctt_size][ctt_frac] = 0
            stalls[ctt_size][ctt_frac] += stall
            count += 1
            if(count == 2):
                break

membw = {}
def extract_membw(file_path, ctt_size, ctt_frac):
    with open(file_path, 'r') as file:
        data = file.readlines()
    
    count = 0
    for line in data:
        # Regex to match your data format
        match = re.match(r"system.mem_ctrls[0-1]\.dram\.bwRead\:\:total[\s]+([0-9]+)([\s]+)", line.strip())
        if match:
            bw = int(match.group(1))
            if ctt_size not in membw:
                membw[ctt_size] = {}  # Initialize empty list for size if not present
            if ctt_frac not in membw[ctt_size]:
                membw[ctt_size][ctt_frac] = 0
            membw[ctt_size][ctt_frac] += bw
            count += 1
            if(count == 2):
                break

def main():
    ctt_sizes = sys.argv[1]
    ctt_fracs = sys.argv[2]
    for size in ctt_sizes.split():
        for frac in ctt_fracs.split():
            extract_time("results/protobuf/protobuf-mc-" + size + "-" + frac + "/system.pc.com_1.device", size, frac)
            extract_stalls("results/protobuf/protobuf-mc-" + size + "-" + frac + "/stats.txt", size, frac)
            extract_membw("results/protobuf/protobuf-mc-" + size + "-" + frac + "/stats.txt", size, frac)

    extract_time("results/protobuf/protobuf/system.pc.com_1.device", 0, 0)
    extract_time("results/protobuf/protobuf-zio/system.pc.com_1.device", 0, 1)

    '''
    if 0 in wall_time:
        if 0 in wall_time[0]:
            print("Baseline\t%d" % (wall_time[0][0]))
        else:
            print("Baseline\tN")
        if 1 in wall_time[0]:
            print("zIO\t%d" % (wall_time[0][1]))
        else:
            print("zIO\tN")

    print("MCSquare", end="\t")
    '''
    print("Figure 20(a) - Wall time")
    for size in ctt_sizes.split():
        print("\t%s" % (size), end="")
    print()
    for frac in ctt_fracs.split():
        print("%d%%" % (float(frac) * 100), end="\t")
        for size in ctt_sizes.split():
            if size in wall_time:
                if frac in wall_time[size]:
                    print("%.1f" % (float(wall_time[size][frac]) / 1000000), end="\t")
                else:
                    print("N", end="\t")
            else:
                print("N", end="\t")
        print()
    print()
    max_stalls = 0
    for size in ctt_sizes.split():
        if size in stalls:
            max_stalls = max(max_stalls, max(stalls[size].values()))
    print("Figure 20(b) - Stalls")
    #print("MCSquare", end="\t")
    for size in ctt_sizes.split():
        print("\t%s" % (size), end="")
    print()
    for frac in ctt_fracs.split():
        print("%d%%" % (float(frac) * 100), end="\t")
        for size in ctt_sizes.split():
            if size in stalls:
                if frac in stalls[size]:
                    print("%.0f%%" % (float(stalls[size][frac] * 100) / max_stalls), end="\t")
                else:
                    print("N", end="\t")
            else:
                print("N", end="\t")
        print()

    bar_labels=["Baseline", "zIO", "(MC)^2"]
    import matplotlib.pyplot as plt
    import numpy as np
    plt.figure(figsize=(8, 4))
    y = np.arange(2)
    height=0.2
    times=[ [wall_time[0][0] / 1000000, cpu_time[0][0] / 1000000],
            [wall_time[0][1] / 1000000, cpu_time[0][1] / 1000000], 
            [wall_time["2048"]["0.5"] / 1000000, cpu_time["2048"]["0.5"] / 1000000]]
    plt.barh(y-0.2, times[0], height)
    plt.barh(y, times[1], height)
    plt.barh(y+0.2, times[2], height)
    plt.yticks(y, ["Wall\nclock\ntime", "CPU time"])
    plt.xlabel('Runtime (ms)')  # Label the x-axis
    plt.legend(bar_labels)
    plt.savefig(sys.argv[3])  # Save the chart to a file

if __name__ == "__main__":
    main()