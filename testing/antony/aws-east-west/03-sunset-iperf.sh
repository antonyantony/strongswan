#!/bin/bash
set -eu

CPUS=$(cat /proc/cpuinfo | grep processor |wc -l)
CPUS=31
PCPU=${PCPU:-"-pcpu"}
duration=${duration:-120}
flows_form=${flows_form:-0}
flows_to=${flows_to:-$CPUS}
eth0=${eth0:-eth0}
dst=${dst:-"192.1.20.252"}
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
                ${TASKSET}$icpu iperf3 -t ${duration} -c ${dst} -p 52${i1} -J > ${iperf_output}/iperf3-52${i1}.json &
        done
        pids=$(pidof iperf3 | wc -l)
	echo "# iperfs started ${pids} " >> ${iperf_output}/${j1}-iperf-pids.txt
        while [ "${pids}" -gt 0 ] ; do
                sleep 5;
                pids=$(pidof iperf3 | wc -l)
        done
	export output=${iperf_output}
        ag=$(./scripts/sunset-post.sh)
        echo "${j1} ${ag}" >> OUTPUT/${host}/tp-table.txt
        echo "${j1} ${ag}"
        ip -d -s link show dev ${eth0} >> ${iperf_output}/${j1}-ip-link-show-dev-${eth0}.txt
done
