#!/bin/bash

DEVICE="0000:03:00.0"
DRIVER="alx"

while getopts ":hd:r:" optname
do
    case "$optname" in
        "h")
            echo "   `basename ${0}`:usage:[-d device_name] [-r driver_name]"
            echo "   where device_name can be one in: {0000:02:00.0,0000:03:00.0},driver_name can be one in: {alx,iwlwifi}"
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


${RTE_SDK}/usertools/dpdk-devbind.py --bind=$DRIVER $DEVICE
