set -eu
/usr/local/sbin/ipsec stop  || echo ""
cp ipsec.secrets /usr/local/etc/ipsec.secrets
cp strongswan.conf /usr/local/etc/strongswan.conf
cp perf2.swanctl.conf /usr/local/etc/swanctl/swanctl.conf
/usr/local/sbin/ipsec start
sleep 2
swanctl --load-conn
