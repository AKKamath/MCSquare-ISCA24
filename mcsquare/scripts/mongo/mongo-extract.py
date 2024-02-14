import re
import sys
import os

wall_time = {}
def extract_time(file_path, ctt_size = 0, ctt_frac = 0):
    if not os.path.isfile(file_path):
        return

    with open(file_path, 'r') as file:
        data = file.readlines()

    for line in data:
        # Regex to match your data format
        match = re.match(r"\[INSERT\], AverageLatency\(us\), ([0-9]+\.[0-9]+)", line.strip())
        if match:
            latency = float(match.group(1))
            if ctt_size not in wall_time:
                wall_time[ctt_size] = {}  # Initialize empty list for size if not present
            wall_time[ctt_size][ctt_frac] = latency

stalls = {}
def extract_stalls(file_path, ctt_size, ctt_frac):
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
            if ctt_size not in stalls:
                stalls[ctt_size] = {}  # Initialize empty list for size if not present
            if ctt_frac not in stalls[ctt_size]:
                stalls[ctt_size][ctt_frac] = 0
            stalls[ctt_size][ctt_frac] += stall
            count += 1
            if(count == 2):
                break

def main():
    ctt_sizes = sys.argv[1]
    ctt_fracs = sys.argv[2]
    iters = sys.argv[3]
    for i in iters.split():
        for size in ctt_sizes.split():
            for frac in ctt_fracs.split():
                extract_time("results/mongo/mc/mongo-100K-mc-all-" + size + "-" + frac + "-" + i + "/system.pc.com_1.device", size, frac)
                extract_stalls("results/mongo/mc/mongo-100K-mc-all-" + size + "-" + frac + "-" + i + "/stats.txt", size, frac)
        extract_time("results/mongo/norm/mongo-100K-" + i + "/system.pc.com_1.device", 0, 0)
        extract_time("results/mongo/zio/mongo-100K-zio-" + i + "/system.pc.com_1.device", 0, 1)

    print("Insertion latency")
    if 0 in wall_time:
        if 0 in wall_time[0]:
            print("Baseline\t%.2f" % (wall_time[0][0]))
        else:
            print("Baseline\tN")
        if 1 in wall_time[0]:
            print("zIO\t%.2f" % (wall_time[0][1]))
        else:
            print("zIO\tN")

    print("MCSquare", end="\t")
    for size in ctt_sizes.split():
        print("%s" % (size), end="\t")
    print()
    for frac in ctt_fracs.split():
        print("%s" % (frac), end="\t")
        for size in ctt_sizes.split():
            if size in wall_time:
                if frac in wall_time[size]:
                    print("%.2f" % (wall_time[size][frac]), end="\t")
                else:
                    print("N", end="\t")
            else:
                print("N", end="\t")
        print()
    print()

    print("Stalls")
    print("MCSquare", end="\t")
    for size in ctt_sizes.split():
        print("%s" % (size), end="\t")
    print()
    for frac in ctt_fracs.split():
        print("%s" % (frac), end="\t")
        for size in ctt_sizes.split():
            if size in stalls:
                if frac in stalls[size]:
                    print("%d" % (stalls[size][frac]), end="\t")
                else:
                    print("N", end="\t")
            else:
                print("N", end="\t")
        print()


if __name__ == "__main__":
    main()