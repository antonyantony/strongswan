#!/bin/bash
set -eu
host=${host:-"sunset"}
tp=0
output=${output:-OUTPUT}

for f in ${output}/iperf3-*.json ; do
	tp1=$(jq '.end.sum_sent.bits_per_second' ${f})
	tp=$(bc <<< "${tp} + ${tp1}")
done
tp=$(bc -l <<< "scale=2; ${tp} / 1024^3")
echo $tp
