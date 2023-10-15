#!/bin/bash
export SUNSET=${SUNSET:-"192.1.10.252"}
export SUNRISE=${SURISE:-"192.1.20.252"}
export WEST0=${WEST0:-"192.1.10.254"}
export EAST0=${EAST0-"192.1.10.254"}
set -eu
CPUS=$(cat /proc/cpuinfo | grep processor | wc -l)
/usr/sbin/ipsec stop || echo ""
cp ipsec.secrets /etc/ipsec.secrets
cp strongswan.conf /etc/strongswan.conf
[ -d /etc/swanctl/ ] || mkdir /etc/swanctl/
cp west.swanctl.conf /etc/swanctl/swanctl.conf
/usr/sbin/ipsec restart
sleep 2
swanctl --load-conn
# need one cpu, say 0, twice for the fallback sa.
taskset 0x1 ping -W 2 -c 1 -I ${WEST0} ${EAST0} || true
sleep 1
for i in $(seq 1 ${CPUS}); do
        taskset 0x$i ping -W 2 -c 1 -I ${WEST0} ${EAST0} || true
        sleep 1
done
