cd /home/akkamath/gem5-zIO/util/m5
scons build/x86/out
cd /home/akkamath/gem5-zIO/mcsquare

pushd lib
make mc_interpose.so
popd

MC_BIN=/home/akkamath/gem5-zIO/mcsquare/lib/mc_interpose.so

pushd /home/akkamath/cicada-engine
mkdir -p build
pushd build
cmake ..
make -j test_tx

# Setup huge pages for cicada
echo "100" > /proc/sys/vm/nr_hugepages

mnthuge=/mnt/huge
echo "Creating $mnthuge and mounting as hugetlbfs"
mkdir -p $mnthuge
grep -s $mnthuge /proc/mounts > /dev/null
if [ $? -ne 0 ] ; then
    mount -t hugetlbfs nodev $mnthuge
fi

cp ../src/mica/test/test_tx.json .

THREADS=4
ROWS=1000
ROW_SIZE=4096
TX=50

./test_tx ${ROWS} 4 0 0 ${TX} ${THREADS} 1 1 ${ROW_SIZE}

echo "Setup complete"
m5 exit

for i in 0.125 0.25 0.5 1; do
    m5 resetstats
    LD_PRELOAD=${MC_BIN} ./test_tx ${ROWS} 4 0 0 ${TX} ${THREADS} ${i} 1 ${ROW_SIZE}
    m5 dumpstats
done

m5 exit