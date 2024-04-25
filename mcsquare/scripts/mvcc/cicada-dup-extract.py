import re
import sys
def extract_thput(file_path):
    with open(file_path, 'r') as file:
        data = file.readlines()

    # Dictionary to hold throughput
    thput = []

    for line in data:
        # Regex to match your data format
        match = re.match(r"throughput:[\s]+([0-9]+\.[0-9]+) M/sec", line.strip())
        if match:
            thput.append(float(match.group(1)) * 1000)
    # Append the max cycles for the last experiment
    return thput

def extract_stalls(file_path):
    with open(file_path, 'r') as file:
        data = file.readlines()

    # Dictionary to hold stalls
    stalls = []
    
    count = 0
    for line in data:
        # Regex to match your data format
        match = re.match(r"system.mem_ctrls[0-9].mcsquare.memElideBlockedCTTFull[\s]+([0-9]+)([\s]+)", line.strip())
        if match:
            stall = int(match.group(1))
            if count % 2 == 0:
                stalls.append(stall)
            else:
                stalls[count // 2] += stall
            count += 1
    return stalls

def extract_membw(file_path):
    with open(file_path, 'r') as file:
        data = file.readlines()

    bw_util = []
    
    count = 0
    for line in data:
        # Regex to match your data format
        match = re.match(r"system.mem_ctrls[0-9]\.dram.busUtil[\s]+([0-9]+\.[0-9]+)([\s]+)", line.strip())
        if match:
            stall = float(match.group(1))
            if count % 2 == 0:
                bw_util.append(stall)
            else:
                bw_util[count // 2] += stall
            count += 1
    return bw_util

def main():
    tests = sys.argv[1].split()
    threads = sys.argv[2].split()

    thput_mc_vals = {}
    thput_vals = {}
    stalls_mc = {}
    bw_mc_vals = {}
    for thread in threads:
        thput = extract_thput("results/mvcc_sweep2/cicada-rmw-" + thread + "T/system.pc.com_1.device")
        thput_vals[thread] = thput[1:]
        thput_mc_vals[thread] = {}
        stalls_mc[thread] = {}
        bw_mc_vals[thread] = {}
        for test in tests:
            thput = extract_thput("results/mvcc_sweep2/cicada-rmw-mc-" + thread + "T-" + test + "/system.pc.com_1.device")
            stall = extract_stalls("results/mvcc_sweep2/cicada-rmw-mc-" + thread + "T-" + test + "/stats.txt")
            bw = extract_membw("results/mvcc_sweep2/cicada-rmw-mc-" + thread + "T-" + test + "/stats.txt")
            thput_mc_vals[thread][test] = thput[1:]
            stalls_mc[thread][test] = stall
            bw_mc_vals[thread][test] = bw

    print("Throughput")
    print("Threads", end="\t")
    print("Base", end="\t")
    for test in tests:
        print(f"{test}", end="\t")
    print()

    for thread in threads:
        print(thread + "T", end="\t")
        if(len(thput_vals[thread]) > 0):
            print("{:d}".format(int(thput_vals[thread][0])), end="\t")
        else:
            print("N", end="\t")
        for test in tests:
            if(len(thput_mc_vals[thread][test]) > 0):
                print("{:d}".format(int(thput_mc_vals[thread][test][0])), end="\t")
            else:
                print("N", end="\t")
        print("")

    print("Stalls")
    print("Threads", end="\t")
    for test in tests:
        print(f"{test}", end="\t")
    print()

    for thread in threads:
        print(thread + "T", end="\t")
        for test in tests:
            if(len(stalls_mc[thread][test]) > 0):
                print("{:d}".format(int(stalls_mc[thread][test][0])), end="\t")
            else:
                print("N", end="\t")
        print("")

    print("Bandwidth")
    print("Threads", end="\t")
    for test in tests:
        print(f"{test}", end="\t")
    print()

    for thread in threads:
        print(thread + "T", end="\t")
        for test in tests:
            if(len(bw_mc_vals[thread][test]) > 0):
                print("{:f}".format(bw_mc_vals[thread][test][0]), end="\t")
            else:
                print("N", end="\t")
        print("")

    '''
    import matplotlib.pyplot as plt
    import numpy as np
    plt.figure(figsize=(8, 4))
    x = np.arange(len(sizes))
    width=1/float(len(labels)) - 0.1
    for test in tests:
        plt.bar(x, thput_mc_vals[test], width)
        x = [i + width for i in x]
    x = [i - width * float(len(tests)) / 2 for i in x]
    plt.xticks(x, [str(float(x) * 100.0) + "%" for x in sizes])
    plt.xlabel('Fraction updated')  # Label the x-axis
    plt.yticks(np.arange(0, max(max(thput_mc_vals[val] for val in thput_mc_vals.keys())) + 50, 50))
    plt.ylabel('Throughput (kOps/sec)')
    plt.legend(labels)
    plt.savefig(sys.argv[4])  # Save the chart to a file
    '''
    

if __name__ == "__main__":
    main()
