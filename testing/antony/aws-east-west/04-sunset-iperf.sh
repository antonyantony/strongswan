#!/bin/bash
set -eu
CPUS0=$(cat /proc/cpuinfo | grep processor |wc -l)
CPUS=${CPUS:-$CPUS0}
PCPU=${PCPU:-"-pcpu"}
duration=${duration:-40}
flows_form=${flows_form:-0}
flows_to=${flows_to:-$CPUS}
eth0=${eth0:-eth0}
SUNRISE=${SUNRISE:-"192.1.20.252"}
dst=${SUNRISE}
host=${host:-"sunset"}
output=OUTPUT/${host}
TASKSET="taskset 0x"

mkdir -p ${output}
rm -f ${output}/tp-table.txt
touch ${output}/tp-table.txt
for j in $(seq "${flows_form}" "${flows_to}"); do
	output=OUTPUT/${host}
        rm -f ${output}/iperf3-*.json
        j1=$((j + 1))
        iperf_output="${output}/iperf-json${PCPU}-${j1}"
        rm -fr ${iperf_output}
        mkdir ${iperf_output}
        ip -d -s link show dev ${eth0} > ${iperf_output}/${j1}-ip-link-show-dev-${eth0}.txt
        for i in $(seq 0 "${j}"); do
                i01=$((i + 1))
                # icpu=$((i + 1 + 53))
                icpu=${i01}
                printf -v i1 "%02d" ${i01}
                ${TASKSET}$icpu iperf3 -t ${duration} -c ${dst} -p 52${i1} -J > ${iperf_output}/iperf3-52${i1}.json&
		echo "$!" > ${iperf_output}/iperf3-52${i1}.pid
        done
	npids=$(ls ${iperf_output}/iperf3-52*.pid | wc -l || echo 0)
	echo "# iperfs started ${npids} "
        while [ "${npids}" -gt 0 ] ; do
                for f in ${iperf_output}/iperf3-52*.pid; do
			pid=$(cat $f)
			pidof iperf3 | grep -w $pid && break || echo ""
			rm $f
		done
		npids=$(ls ${iperf_output}/iperf3-52*.pid | wc -l || echo 0)
		sleep 5
        done
	export output=${iperf_output}
        ag=$(./scripts/sunset-post.sh)
        echo "${j1} ${ag}" >> OUTPUT/${host}/tp-table.txt
        echo "${j1} ${ag}"
        ip -d -s link show dev ${eth0} >> ${iperf_output}/${j1}-ip-link-show-dev-${eth0}.txt
done
