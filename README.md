
# $(MC)^{2}$: Lazy <ins>M</ins>em<ins>C</ins>opy at the <ins>M</ins>emory <ins>C</ins>ontroller

We provide the source code and setup necessary for $(MC)^{2}$. 
$(MC)^{2}$ is a hardware extension that provides support for a lazy memcpy operation. 

This operation avoids copying data at the time of function call.  Instead, if copied destinations are later accessed, $(MC)^{2}$ uses tracking information to seamlessly reroute the request to the appropriate source, while lazily executing copies only when necessary. $(MC)^{2}$ modifies the memory controller and has been implemented using gem5, a CPU simulator.

This repository consists of the source code of the simulator and all scripts needed to replicate the figures in the paper.

The original readme for gem5 has been retained as [README](README).

We shall first explain how to replicate our results, then highlight the important files and folders contained in this repository.

## Setting up gem5
Detailed instructions on building gem5 and alternative docker containers can be found on their [webpage](https://www.gem5.org/documentation/general_docs/building).

Ensure that your machine supports KVM, as this is used during our simulations. This can be done by running the following command and ensuring the return value is 1 or more:
```bash
egrep -c '(vmx|svm)' /proc/cpuinfo
```

Ubuntu 20.04 or 22.04 is preferred.
The following commands installs all the dependencies for Ubuntu 22.04:
```bash
sudo apt install build-essential git m4 scons zlib1g zlib1g-dev \
    libprotobuf-dev python3-dev protobuf-compiler libprotoc-dev \
    qemu-kvm libvirt-daemon-system libgoogle-perftools-dev \
    libboost-all-dev pkg-config python3-tk libvirt-clients \
    bridge-utils python3-matplotlib python3-numpy
```

## Running all figures and generating outputs 

Once the setup for gem5 is completed, proceed with the following steps to run the artifact. 
To run all the benchmarks and generate figures run:
```bash
make launch_all 
```
To regenerate all outputs run: 
```bash
make extract_all
```
The outputs will be generated as two seperate files. A TXT file is generated in results/figure_X.txt, where X is the specific figure number, which contains the raw numbers to be plotted. figures/figure_X.png will contain the plotted figure. The exception is for Figure 20, where only a TXT file containing the table is generated.
Minor variances in performance numbers occur from run to run, but general trends should remain stable.

## Running figures individually

Alternatively, individual figures can be obtained by running the following commands:
```bash
make launch_micro_latency   #Figure 10:   10 min
make launch_micro_breakdown #Figure 11:   10 min
make launch_micro_seq       #Figure 12:   30 min
make launch_micro_rand      #Figure 13:    1 hr
make launch_protobuf        #Figure 14,20: 2 hr
make launch_mongo           #Figure 15:   15 hr
make launch_mvcc            #Figure 16,17: 6 hr
make launch_hugepage_access #Figure 18    20 min
make launch_pipe            #Figure 19:   15 min
make launch_src_write       #Figure 21:   10 min
```


## Source code
There majority of this repository matches gem5. A new folder called [mcsquare](./mcsquare) contains files required for execution. It contains the following subfolders:
- **[lib](mcsquare/lib/)**: This folder contains the runtime code for $(MC)^{2}$, consisting of a header file (mcsquare.h) containing the function for lazy memcpy (memcpy_elide_clwb), and the library interposers. These files are used within simulation to convert benchmarks from standard memcpy to lazy memcpy. 
- **[scripts](mcsquare/scripts/)**: This folder contains bash and python scripts for executing experiments and plotting the different graphs.
- **[results](mcsquare/results/)**: This folder is generated on running an experiment, and contains the raw output for the experiment.
- **[figures](mcsquare/figures/)**: This folder is generated on completing an experiment, and contains the plotted figure for the experiment.

The functionality for (MC)$^{2}$ is encapsulated in [src/mem/mcsquare.cc](src/mem/mcsquare.cc), which contains the implementation of the tables and buffers required. Other modifications were performed in the [memory controller](src/mem/mem_ctrl.cc) and [memory interconnect](src/mem/coherent_xbar.cc) to support this new feature.
