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

./test_tx ${ROWS} 4 0 0 ${TX} ${THREADS} 1 0 ${ROW_SIZE}

echo "Setup complete"
m5 exit

for i in 0.125 0.25 0.5 1; do
    m5 resetstats
    ./test_tx ${ROWS} 4 0 0 ${TX} ${THREADS} ${i} 0 ${ROW_SIZE}
    m5 dumpstats
done

m5 exit