
apt-get -o Acquire::Check-Valid-Until=false -o Acquire::Check-Date=false update
apt-get install software-properties-common
add-apt-repository universe
apt-get update

# GEM5 requirements
apt install build-essential git m4 scons zlib1g zlib1g-dev \
    libprotobuf-dev protobuf-compiler libprotoc-dev libgoogle-perftools-dev \
    python3-dev python-is-python3 libboost-all-dev pkg-config

# zIO requirements
apt install build-essential make pkg-config autoconf libnuma-dev libaio1 \
    libaio-dev uuid-dev librdmacm-dev ndctl numactl libncurses-dev libssl-dev \
    libelf-dev rsync

# Needed for gem5 setup
apt install wget openjdk-17-jdk openjdk-17-jre

# Setup m5 commands
wget http://cs.wisc.edu/~powerjg/files/gem5-guest-tools-x86.tgz
tar xzvf gem5-guest-tools-x86.tgz
pushd gem5-guest-tools/
./install
popd

mkdir home/akkamath
pushd home/akkamath
git clone https://github.com/AKKamath/gem5-zIO.git