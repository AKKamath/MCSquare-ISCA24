#!/bin/bash
COLUMNS=$1
ROWS=$2
CYCLES=($(grep -e "switch_cpus.numCycles" results/stats.txt | grep -oE "[0-9]+"));

printf "Runtime\t"
for i in ${COLUMNS}; do
    printf "$i\t"
done
printf "\n"

num_cols=${#COLUMNS[@]}

it=0
for j in ${ROWS}; do
    printf "$j\t"
    for i in ${COLUMNS}; do
        printf "${CYCLES[${it}]}\t"
        it=$((it+1))
    done
    printf "\n"
done
printf "\n"