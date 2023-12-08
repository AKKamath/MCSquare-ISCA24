ZIO=/home/akkamath/zIO
ZIO_BIN=${ZIO}/copy_interpose_manual.so

pushd ${ZIO};
make
ls
popd

cd /fleetbench/bazel-bin/fleetbench;

m5 exit

m5 resetstats
LD_PRELOAD=${ZIO_BIN} ./proto/proto_benchmark --benchmark_min_time=0.01s --benchmark_max_time=0.01s
m5 dumpstats

m5 exit