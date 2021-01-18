#*********************** DPDK lib operations
yum install pciutils

ldconfig ${RTE_SDK}/${RTE_TARGET}/lib

# remove module 
rmmod rte_kni
rmmod igb_uio
rmmod uio

# insert module
sudo modprobe uio
sudo insmod ${RTE_SDK}/${RTE_TARGET}/kmod/igb_uio.ko
sudo insmod ${RTE_SDK}/${RTE_TARGET}/kmod/rte_kni.ko


#*********************** HUGEPAGES operations

# cleanup huge page
echo 0 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages

# create mount point
mkdir -p /mnt/huge
mount -t hugetlbfs nodev /mnt/huge

# setup
echo 5120 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages

#*********************** NIC operations

# nic unbind
${RTE_SDK}/usertools/dpdk-devbind.py -u 0000:08:00.0 0000:08:00.1 

# nic bind
${RTE_SDK}/usertools/dpdk-devbind.py --bind=igb_uio 0000:08:00.0 0000:08:00.1 

# show bind
${RTE_SDK}/usertools/dpdk-devbind.py -s
