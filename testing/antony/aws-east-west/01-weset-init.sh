set -eu
CPUS=$(cat /proc/cpuinfo   | grep processor |wc -l)
/usr/local/sbin/ipsec stop || echo ""
cp ipsec.secrets /usr/local/etc/ipsec.secrets
cp strongswan.conf /usr/local/etc/strongswan.conf
cp west.swanctl.conf /usr/local/etc/swanctl/swanctl.conf
/usr/local/sbin/ipsec restart
sleep 2
swanctl --load-conn
# set source addrss
ip route add  192.0.2.0/24 via 192.1.2.23 src 192.0.1.254 || echo "route exits"
ip route change  192.0.2.0/24 via 192.1.2.23 src 192.0.1.254 || echo "route missing"
# prime the ipsec tunnels
# need one cpu, say 0, twice for the head sa.
taskset 0x1 ping -W 2 -c 1 -I 192.0.1.254 192.0.2.254 || true
sleep 1
for i in $(seq 1 ${CPUS}); do
        taskset 0x$i ping -W 2 -c 1 -I 192.0.1.254 192.0.2.254 || true
        sleep 1
done
