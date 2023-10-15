set -eu
/usr/sbin/ipsec stop  || echo ""
cp ipsec.secrets /etc/ipsec.secrets
cp strongswan.conf /etc/strongswan.conf
[ -d /etc/swanctl/ ] || mkdir /etc/swanctl/
cp east.swanctl.conf /etc/swanctl/swanctl.conf
/usr/sbin/ipsec restart
sleep 2
swanctl --load-conn
