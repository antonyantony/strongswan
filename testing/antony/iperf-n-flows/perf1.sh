set -eu
/usr/local/sbin/ipsec stop  || echo ""
cp ipsec.secrets /usr/local/etc/ipsec.secrets
cp strongswan.conf /usr/local/etc/strongswan.conf
cp perf1.swanctl.conf /usr/local/etc/swanctl/swanctl.conf
/usr/local/sbin/ipsec start
sleep 2
swanctl --load-conn
# set source addrss
ip route change 10.1.1.0/24 via 192.168.1.101 dev eth4 src 10.0.1.1 || true
# prime the ipsec tunnels

# need one cpu, say 0, twice for the head sa.
numactl -C 0 ping -W 2 -c 1 -I 10.0.1.1 10.1.1.1 || true
for i in $(seq 0 17); do
	numactl -C $i ping -W 2 -c 1 -I 10.0.1.1 10.1.1.1 || true
	sleep 1
done
