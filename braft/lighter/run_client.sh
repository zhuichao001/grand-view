#!/bin/bash

# source shflags from current directory
mydir="${BASH_SOURCE%/*}"
if [[ ! -d "$mydir" ]]; then mydir="$PWD"; fi
. $mydir/../shflags


# define command-line flags
DEFINE_boolean clean 1 'Remove old "runtime" dir before running'
DEFINE_integer add_percentage 80 'Percentage of incrby operation'
DEFINE_integer bthread_concurrency '8' 'Number of worker pthreads'
DEFINE_integer server_port 8100 "Port of the first server"
DEFINE_integer server_num '3' 'Number of servers'
DEFINE_integer thread_num 3 'Number of sending thread'
DEFINE_string crash_on_fatal 'true' 'Crash on fatal log'
DEFINE_string log_each_request 'true' 'Print log for each request'
DEFINE_string valgrind 'false' 'Run in valgrind'
DEFINE_string use_bthread "true" "Use bthread to send request"

FLAGS "$@" || exit 1

# hostname prefers ipv6
IP=`hostname -i | awk '{print $NF}'`

if [ "$FLAGS_valgrind" == "true" ] && [ $(which valgrind) ] ; then
    VALGRIND="valgrind --tool=memcheck --leak-check=full"
fi

raft_peers=""
for ((i=0; i<$FLAGS_server_num; ++i)); do
    raft_peers="${raft_peers}${IP}:$((${FLAGS_server_port}+i)):0,"
done

export TCMALLOC_SAMPLE_PARAMETER=524288

${VALGRIND} ./lighter_client \
        --add_percentage=${FLAGS_add_percentage} \
        --bthread_concurrency=${FLAGS_bthread_concurrency} \
        --conf="${raft_peers}" \
        --crash_on_fatal_log=${FLAGS_crash_on_fatal} \
        --log_each_request=${FLAGS_log_each_request} \
        --thread_num=${FLAGS_thread_num} \
        --use_bthread=${FLAGS_use_bthread} \
