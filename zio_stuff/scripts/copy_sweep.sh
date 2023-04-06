#!/bin/bash

THREADS=4
TIMEOUT=10
ZIO=/home/akkamath/zIO
ZIO_BIN=../../tas/lib/copy_interpose.so
BYTE_LIST="524288"

#This is the copy_sweep directory
cd ../benchmarks/micro_rpc_cpy
make
mkdir -p results/copy_sweep/;

# Remove any data from previous runs
rm results/copy_sweep/*.dat

#Perform the Linux experiments first
echo "Linux Runs"

#This is how we run the microbenchmark. 
#The first command line option is the number of threads. The second is the bytes. The third is time.
for BYTES in ${BYTE_LIST}; do
    ./copy_sweep ${THREADS} ${BYTES} ${TIMEOUT} >> results/copy_sweep/${BYTES}_copy.dat
done

#This section will have the zIO experiments.
echo "zIO Runs"
for BYTES in ${BYTE_LIST}; do
    LD_PRELOAD=${ZIO_BIN} ./copy_sweep ${THREADS} ${BYTES} ${TIMEOUT} >> results/copy_sweep/${BYTES}_copy_zio.dat
done

#After all the different server configurations are done, we run a simple script on the client machine to parse the output, cut the warmup period and get the average of the run. 
: '
echo "Processing..."
cd ${ZIO}/benchmarks/micro_rpc_cpy/results/copy_sweep; ./process.sh

#The processing script summarizes the results in a final.dat file, which we can retrieve. 
cp ${ZIO}/benchmarks/micro_rpc_cpy/results/copy_sweep/final.dat .
cat final.dat
rm final.dat
'
tail -n 6 results/copy_sweep/*