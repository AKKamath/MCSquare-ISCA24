cd /fleetbench/bazel-bin/fleetbench;

m5 exit

m5 resetstats
./proto/proto_benchmark --benchmark_min_time=0.25s --benchmark_max_time=0.25s
m5 dumpstats

m5 exit