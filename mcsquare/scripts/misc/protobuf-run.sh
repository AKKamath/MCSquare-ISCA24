cd /home/akkamath/gem5-zIO/util/m5
scons build/x86/out
cd /home/akkamath/gem5-zIO/mcsquare

pushd lib
make
popd

MC_BIN=/home/akkamath/gem5-zIO/mcsquare/lib/mc_interpose.so

ZIO=/home/akkamath/zIO
ZIO_BIN=${ZIO}/copy_interpose.so

pushd ${ZIO};
make
ls
popd


#m5 exit
cd /fleetbench/bazel-bin/fleetbench;

m5 exit

m5 resetstats
LD_PRELOAD=${MC_BIN} ./proto/proto_benchmark --benchmark_min_time=0.01s --benchmark_max_time=0.01s
m5 dumpstats

m5 resetstats
LD_PRELOAD=${ZIO_BIN} ./proto/proto_benchmark --benchmark_min_time=0.01s --benchmark_max_time=0.01s
m5 dumpstats

m5 resetstats
./proto/proto_benchmark --benchmark_min_time=0.01s --benchmark_max_time=0.01s
m5 dumpstats

m5 exit