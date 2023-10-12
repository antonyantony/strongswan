set -eu
CPUS=$(cat /proc/cpuinfo   | grep processor |wc -l)
/usr/sbin/ipsec stop || echo ""
cp ipsec.secrets /etc/ipsec.secrets
cp strongswan.conf /etc/strongswan.conf
[ -d /etc/swanctl/ ] || mkdir /etc/swanctl/
cp west.swanctl.conf /etc/swanctl/swanctl.conf
/usr/sbin/ipsec restart
sleep 2
swanctl --load-conn
# need one cpu, say 0, twice for the head sa.
taskset 0x1 ping -W 2 -c 1 -I 192.1.10.254 192.1.20.254 || true
sleep 1
for i in $(seq 1 ${CPUS}); do
        taskset 0x$i ping -W 2 -c 1 -I 192.1.10.254 192.1.20.254 || true
        sleep 1
done
