#!/bin/bash
set -eu

PCPU=${PCPU:-""}
duration=${duration:-120}
flows_form=${flows_form:-0}
flows_to=${flows_to:-17}

mkdir -p output/
rm -f output/tp-table.txt
touch output/tp-table.txt
for j in $(seq "${flows_form}" "${flows_to}"); do
	rm -f output/iperf3-*.json
	j1=$((j + 1))
	iperf_output="output-iperf-json${PCPU}-${j1}"
	rm -fr ${iperf_output}
	mkdir ${iperf_output}
	ip -d -s link show dev eth4 > ${iperf_output}/${j1}-ip-link-show-dev-eth4.txt
	X=$(ip -o x s | wc -l)
	X1=$(ip x s | grep cpu| wc -l)
	echo "# before the iperf SAs $X with CPU ID $X1" >  ${iperf_output}/${j1}-ip-xfrm-state.txt
	ip x s >> ${iperf_output}/${j1}-ip-xfrm-state.txt
	echo "# end before the iperf" >>  ${iperf_output}/${j1}-ip-xfrm-state.txt
	echo "# before the iperf" >  ${iperf_output}/${j1}-ip-xfrm-policy.txt
	ip x p > ${iperf_output}/${j1}-ip-xfrm-policy.txt
	echo "# end before the iperf" >> ${iperf_output}/${j1}-ip-xfrm-policy.txt
	for i in $(seq 0 "${j}"); do
		i01=$((i + 1))
		printf -v i1 "%02d" ${i01}
		numactl -C $i iperf3 -t ${duration} -c 10.1.1.1 -p 52${i1} -J > output/iperf3-520${i1}.json &
	done
	pids=$(pidof iperf3 | wc -l)
	while [ "${pids}" -gt 0 ] ; do
		sleep 5;
		pids=$(pidof iperf3 | wc -l)
	done
	ag=$(./perf1.post.sh)
	echo "${j1} ${ag}" >> output/tp-table.txt
	echo "${j1} ${ag}"
	mv output/*json ${iperf_output}/
	ip x s >> ${iperf_output}/${j1}-ip-xfrm-state.txt
	ip x s >> ${iperf_output}/${j1}-ip-xfrm-policy.txt
	ip -d -s link show dev eth4 >> ${iperf_output}/${j1}-ip-link-show-dev-eth4.txt
done
