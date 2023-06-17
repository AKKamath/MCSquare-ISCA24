#!/bin/bash
latencies=($(grep -e "switch_cpus.numCycles" $1 | grep -oE "[0-9]+"));
#echo $latencies
num=3
printf "clflush\tclflush_src\tmemcpy\n"
for ((i = 0; i < ${num}; i++)); do
    printf "%d\t" ${latencies[i]}
done
printf "\n"