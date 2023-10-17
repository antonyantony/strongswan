#!/bin/bash
set -eu
host=${host:-"sunset"}
tp=0
prefix=""
output=${output:-OUTPUT}

for f in ${output}/iperf3-*.json ; do
	tp1=$(jq '.end.sum_sent.bits_per_second' ${f})
	tp=$(bc <<< "${tp} + ${tp1}")
done
tpint=$(printf '%.0f' "${tp}")

if [ "$tpint" -gt 1073741824 ] ; then
	tp=$(bc -l <<< "scale=2; ${tp} / 1024^3")
	prefix=" Gbps"
fi
echo $tp $prefix
