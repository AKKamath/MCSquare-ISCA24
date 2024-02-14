pushd /home/akkamath/cicada-engine
mkdir -p build
pushd build
cmake ..
make -j test_tx

# Setup huge pages for cicada
echo "160" > /proc/sys/vm/nr_hugepages

mnthuge=/mnt/huge
echo "Creating $mnthuge and mounting as hugetlbfs"
mkdir -p $mnthuge
grep -s $mnthuge /proc/mounts > /dev/null
if [ $? -ne 0 ] ; then
    mount -t hugetlbfs nodev $mnthuge
fi

cp ../src/mica/test/test_tx.json .

ROWS=10000
ROW_SIZE=4096
THREADS=4
TX=2000

./test_tx ${ROWS} 4 0 0 ${TX} ${THREADS} 1 0 ${ROW_SIZE}
sleep 5
echo "Setup complete"
m5 exit

m5 exit