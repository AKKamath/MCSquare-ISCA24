MONGO=/home/akkamath/mongo/mongodb-linux-x86_64-ubuntu2004-4.4.12/bin
YCSB=/home/akkamath/YCSB/ycsb-mongodb

mkdir -p /data/db

# Launch mongo server
pushd ${MONGO}
echo "Launching mongo server"
./mongod &
echo "Launched mongo server"
popd
sleep 30

m5 exit

# Launch YCSB
pushd ${YCSB}
echo "Running YCSB"
#m5 resetstats
# -p fieldlength=100000 
# Manually run YCSB java command instead of unnecessary overheads from Python
#./bin/ycsb load mongodb -s -P workloads/workloada
java -cp /home/akkamath/YCSB/ycsb-mongodb/mongodb/target/mongodb-binding-0.1.4.jar:/home/akkamath/YCSB/ycsb-mongodb/mongodb/target/archive-tmp/mongodb-binding-0.1.4.jar:/home/akkamath/YCSB/ycsb-mongodb/core/target/core-0.1.4.jar com.yahoo.ycsb.Client \
        -db com.yahoo.ycsb.db.MongoDbClient -s -P workloads/workloada -load -p fieldlength=10 -p recordcount=50
#m5 dumpstats
echo "YCSB done"
popd

m5 exit
pkill mongod