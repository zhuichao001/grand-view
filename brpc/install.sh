
#prepare environment as root
yum install epel-release
yum install git gcc-c++ make openssl-devel
yum install gflags-devel protobuf-devel protobuf-compiler leveldb-devel
yum install gperftools-devel
yum install gtest-devel

#git clone source code
git clone git@github.com:apache/incubator-brpc.git
sh config_brpc.sh --headers=/usr/include --libs=/usr/lib64
make
cp -r output ../
