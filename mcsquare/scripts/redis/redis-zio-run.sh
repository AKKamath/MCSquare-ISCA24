ZIO=/home/akkamath/zIO
ZIO_BIN=${ZIO}/copy_interpose.so

pushd ${ZIO};
make
ls
popd

REDIS=/home/akkamath/zIO/benchmarks/redis
pushd ${REDIS}
make MALLOC=libc
mkdir pmem
ls
popd

echo "Done compilation"
m5 exit

cd ${REDIS}/src
LD_PRELOAD=${ZIO_BIN} ./redis-server ../redis_ext4.conf &
sleep 1
echo "Redis running"
m5 resetstats
./redis-benchmark -p 7379 -d 16384 -t set -c 1 --csv -n 500
m5 dumpstats
echo "Redis done"
pkill redis-server
sleep 1
m5 exit