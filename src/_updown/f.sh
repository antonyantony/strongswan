IPSEC_POLICY="-m policy --pol ipsec --proto $PLUTO_PROTO --reqid $PLUTO_REQID"
IPSEC_POLICY_IN="$IPSEC_POLICY --dir in"
IPSEC_POLICY_OUT="$IPSEC_POLICY --dir out"
# -A FORWARD -s 192.0.1.0/24 -d 192.0.2.0/24 -i eth1 -m policy --dir in --pol ipsec --reqid 1 --proto esp -j ACCEPT
# -A FORWARD -s 192.0.2.0/24 -d 192.0.1.0/24 -o eth1 -m policy --dir out --pol ipsec --reqid 1 --proto esp -j ACCEPT

PLUTO_INTERFACE=eth1
PLUTO_PEER_PROTOCOL=esp
PLUTO_MY_CLIENT="192.0.2.0/24"
PLUTO_PEER_CLIENT="192.0.1.0/24"
PLUTO_REQID=1

nft -f - << EOF
flush ruleset
table inet strongswan-forward {
        chain forward {
                type filter hook forward priority filter; policy drop;
		ipsec in reqid $PLUTO_REQID iifname $PLUTO_INTERFACE ip saddr $PLUTO_PEER_CLIENT ip daddr $PLUTO_MY_CLIENT counter log  accept
		ipsec out reqid $PLUTO_REQID oifname $PLUTO_INTERFACE ip saddr $PLUTO_MY_CLIENT ip daddr $PLUTO_PEER_CLIENT counter log  accept
		counter
	}
}
EOF
