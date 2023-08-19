REDIS=/home/akkamath/zIO/benchmarks/redis
pushd ${REDIS}
make MALLOC=libc
mkdir pmem
ls
popd

echo "Done compilation"
m5 exit

cd ${REDIS}/src

./redis-server ../redis_ext4.conf &
sleep 1
echo "Redis running"
m5 resetstats
./redis-benchmark -p 7379 -d 16384 -t set -c 16 -n 1000
m5 dumpstats
echo "Redis done"
pkill redis-server
sleep 1
m5 exit