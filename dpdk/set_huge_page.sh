CE="enp3s0"
DRIVER="igb_uio"

while getopts ":hd:r:" optname
do
    case "$optname" in
        "h")
            echo "   `basename ${0}`:usage:[-d device_name] [-r driver_name]"
            echo "   where device_name can be one in: {enp3s0,wlp2s0},driver_name can be one in: {igb_uio,rte_kni}"
            exit 1
            ;;
        "d")
            DEVICE=$OPTARG
            ;;
        "r")
            DRIVER=$OPTARG
            ;;
        *)
            # Should not occur
            echo "Unknown error while processing options"
            ;;
    esac
done

mkdir -p /mnt/huge
mount -t hugetlbfs nodev /mnt/huge
echo 64 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages

ifconfig $DEVICE down

modprobe uio
insmod ${RTE_SDK}/${RTE_TARGET}/kmod/$DRIVER.ko

${RTE_SDK}/usertools/dpdk-devbind.py --bind=$DRIVER $DEVICE
