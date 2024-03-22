import re
import sys

wall_time = {}
cpu_time = {}
def extract_time(file_path, ctt_size = 0, ctt_frac = 0):
    with open(file_path, 'r') as file:
        data = file.readlines()

    count = 0
    for line in data:
        # Regex to match your data format
        match = re.match(r"throughput:[\s]+([0-9]+\.[0-9]+) M/sec", line.strip())
        if match:
            cpu = float(match.group(1))
            if ctt_size not in cpu_time:
                cpu_time[ctt_size] = {}  # Initialize empty list for size if not present
            count += 1
            if(count == 2):
                cpu_time[ctt_size][ctt_frac] = cpu
                return

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
            extract_time("results/mvcc_sweep/cicada-rmw-mc-4K-" + size + "-" + frac + "/system.pc.com_1.device", size, frac)
            extract_stalls("results/mvcc_sweep/cicada-rmw-mc-4K-" + size + "-" + frac + "/stats.txt", size, frac)
            extract_membw("results/mvcc_sweep/cicada-rmw-mc-4K-" + size + "-" + frac + "/stats.txt", size, frac)

    extract_time("results/mvcc_sweep/cicada-rmw-4K/system.pc.com_1.device", 0, 0)

    print("Throughput")
    if 0 in cpu_time:
        if 0 in cpu_time[0]:
            print("Baseline\t%.3f" % (cpu_time[0][0]))
        else:
            print("Baseline\tN")

    print("MCSquare", end="\t")
    for size in ctt_sizes.split():
        print("%s" % (size), end="\t")
    print()
    for frac in ctt_fracs.split():
        print("%s" % (frac), end="\t")
        for size in ctt_sizes.split():
            if size in cpu_time:
                if frac in cpu_time[size]:
                    print("%.3f" % (cpu_time[size][frac]), end="\t")
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

    print("Membw")
    print("MCSquare", end="\t")
    for size in ctt_sizes.split():
        print("%s" % (size), end="\t")
    print()
    for frac in ctt_fracs.split():
        print("%s" % (frac), end="\t")
        for size in ctt_sizes.split():
            if size in membw:
                if frac in membw[size]:
                    print("%d" % (membw[size][frac]), end="\t")
                else:
                    print("N", end="\t")
            else:
                print("N", end="\t")
        print()


if __name__ == "__main__":
    main()