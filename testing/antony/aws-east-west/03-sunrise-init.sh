PCPU=${PCPU:-"pcpu"}
host=${host:-"surise"}
output=OUTPUT/${host}
mkdir -p ${output}
CPUS=$(cat /proc/cpuinfo   | grep processor |wc -l)
CPUS=36
flows_to=$CPUS
flows_form=${flows_form:-5200}
flows_to=$((5200 + CPUS))
for i in $(seq "${flows_form}" "${flows_to}"); do
	iperf3 -D -s -p $i
done
