set -eu
/usr/local/sbin/ipsec stop  || echo ""
cp ipsec.secrets /usr/local/etc/ipsec.secrets
cp strongswan.conf /usr/local/etc/strongswan.conf
cp east.swanctl.conf /usr/local/etc/swanctl/swanctl.conf
/usr/local/sbin/ipsec restart
sleep 2
swanctl --load-conn
