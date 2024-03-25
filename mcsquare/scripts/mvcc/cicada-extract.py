import re
import sys
'''
	printf "Throughput\t"; \
	for FRAC in ${MVCC_FRAC}; do \
		printf "$${FRAC}\t"; \
	done;
	printf "\n";

	for SIZE in ${MVCC_SIZES}; do \
		for TEST in ${MVCC_TESTS}; do \
			printf "cicada-$${TEST}\t"; \
			thput=$$(grep "throughput" ${RESULTS}/mvcc/cicada-$${TEST}/system.pc.com_1.device | grep -oE "[0-9]+\.[0-9]+" | tail -n +2); \
			for i in $${thput}; do \
				printf "$${i}\t"; \
			done; \
			printf "\n"; \
		done; \
	done;
'''
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

def main():
    tests = sys.argv[1].split()
    labels = sys.argv[2].split()
    sizes = sys.argv[3].split()

    throughput_vals = {}
    for test in tests:
        thput = extract_thput("results/mvcc/cicada-" + test + "/system.pc.com_1.device")
        throughput_vals[test] = thput[1:]

    print("Frac updated", end="\t")
    for index in range(len(tests)):
        print(f"{labels[index]}", end="\t")
    print()

    for index in range(len(sizes)):
        print((float(sizes[index]) * 100), end="%\t")
        for test in tests:
            if(len(throughput_vals[test]) > index):
                print("{:d}".format(int(throughput_vals[test][index])), end="\t")
        print("")

    import matplotlib.pyplot as plt
    import numpy as np
    plt.figure(figsize=(8, 4))
    x = np.arange(len(sizes))
    width=1/float(len(labels)) - 0.1
    for test in tests:
        plt.bar(x, throughput_vals[test], width)
        x = [i + width for i in x]
    x = [i - width * float(len(tests)) / 2 for i in x]
    plt.xticks(x, [str(float(x) * 100.0) + "%" for x in sizes])
    plt.xlabel('Fraction updated')  # Label the x-axis
    plt.yticks(np.arange(0, max(max(throughput_vals[val] for val in throughput_vals.keys())) + 50, 50))
    plt.ylabel('Throughput (kOps/sec)')
    plt.legend(labels)
    plt.savefig(sys.argv[4])  # Save the chart to a file
    

if __name__ == "__main__":
    main()
