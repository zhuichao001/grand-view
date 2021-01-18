sudo yum install libpcap
sudo yum install libpcap-devel

mkdir install && cd install
wget http://fast.dpdk.org/rel/dpdk-18.11.10.tar.xz
xz -d dpdk-18.11.10.tar.xz 
tar -xf dpdk-18.11.10.tar 

sudo su
export RTE_SDK=/opt/dpdk-18.11
export RTE_TARGET=x86_64-native-linuxapp-gcc
mv dpdk-stable-18.11.10/ $RTE_SDK
cd $RTE_SDK

sed -ri 's,(PMD_PCAP=).*,\1y,' config/common_base
sed -ri 's,(BUILD_SHARED_LIB=).*,\1y,' config/common_base
make config T=$RTE_TARGET

sudo make  install T=$RTE_TARGET DESTDIR=/usr/local
