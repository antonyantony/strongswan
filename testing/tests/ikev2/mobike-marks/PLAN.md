# Plan: mobike-mark-in-out

## Topology
`alice (eth0 only) --- moon --- winnetou --- sun --- bob`
Diagram: `a-m-w-s-b.png`

alice: eth0 = 10.1.0.10 (always behind moon NAT, single interface, no virtual IP)
moon:  NAT 10.1.0.0/16 -> PH_IP_MOON (port range changes mid-test)

## Scenario

1. moon NAT set up with port range 1024-1100
2. alice (10.1.0.10) connects to sun through moon NAT, no DPD (default)
3. sun assigns mark_in=2, mark_out=3, set_mark_in=6, set_mark_out=5 to
   alice's child SA (masks: mark_in 0xffffaaaa, mark_out 0xffffbbbb,
   set_mark_in 0xffff6666, set_mark_out 0xffffdddd)
4. sun iptables mangle marks return traffic (bob->alice) with mark 3 so
   it matches the outbound XFRM policy (mark_out=3) and gets encrypted
5. Verify SA established: sun shows mark-in=00000002 mark-out=00000003
6. Verify ping bob works
7. moon: flush NAT rules, add new rule with port range 5000-5100
8. moon: conntrack -F  (flush conntrack so existing mapping is gone)
9. alice: send a ping -- ESP/IKE goes through moon with new port
10. sun detects remote endpoint changed, uses XFRM_MSG_MIGRATE_STATE to migrate SA
11. Verify marks preserved: sun still shows mark-in=00000002 mark-out=00000003
12. Verify ip xfrm state on sun shows all four marks preserved:
    mark 0x2/0xffffaaaa (mark_in), mark 0x3/0xffffbbbb (mark_out),
    output-mark 0x6/0xffff6666 (set_mark_in), output-mark 0x5/0xffffdddd (set_mark_out)
13. Verify ping bob still works after migration (functional mark check:
    if mark lost, mangle rule won't match and bob replies won't be encrypted)

## Files

| File | Based on | Changes |
|------|----------|---------|
| `test.conf` | mobike-nat-mappings | identical |
| `description.txt` | -- | new |
| `pretest.dat` | mobike-nat-mappings | add sun mangle rule; no tcp NAT rule needed |
| `posttest.dat` | mobike-nat-mappings | add sun::iptables -t mangle -F |
| `evaltest.dat` | mobike-nat-mappings | no DPD sleep/log checks; add mark checks; add ip xfrm state check |
| `hosts/alice/etc/strongswan.conf` | mobike-nat-mappings | identical |
| `hosts/alice/etc/iptables.rules` | mobike-nat-mappings | identical |
| `hosts/alice/etc/swanctl/swanctl.conf` | mobike-nat-mappings | remove vips and dpd_delay |
| `hosts/sun/etc/strongswan.conf` | mobike-nat-mappings | identical |
| `hosts/sun/etc/iptables.rules` | mobike-nat-mappings | identical |
| `hosts/sun/etc/swanctl/swanctl.conf` | mobike-nat-mappings | remove pools; add mark_in/set_mark_in/mark_out/set_mark_out to child |

## swanctl changes

alice -- remove vips, no dpd_delay (default is off):
```
connections {
   mobike {
      remote_addrs = PH_IP_SUN
      ...
      children {
         mobike {
            remote_ts = 10.2.0.0/16
         }
      }
   }
}
```

sun -- add marks, remove pools:
```
connections {
   mobike {
      local_addrs = PH_IP_SUN
      ...
      children {
         mobike {
            local_ts     = 10.2.0.0/16
            mark_in      = 2/0xffffaaaa
            mark_out     = 3/0xffffbbbb
            set_mark_in  = 6/0xffff6666
            set_mark_out = 5/0xffffdddd
         }
      }
   }
}
```

Values 2-5 cover all four mark attributes (XFRMA_MARK inbound, XFRMA_MARK
outbound, XFRMA_SET_MARK inbound, XFRMA_SET_MARK outbound) with distinct
values and distinct masks so any mix-up is immediately visible in ip xfrm state.

Mask constraint: set_mark_in mask must have bit1 set so that
(set_mark_in.v & set_mark_in.m) & mark_in.m == mark_in.v.
0xffff6666 (0x66=0110 0110) has bit1=1: (6 & 0xffff6666)=6, 6 & 0xffffaaaa=2 ✓.
(0xffffcccc fails: 0xcc=1100 1100, bit1=0, so 6 & 0xffffcccc=4, 4 & 0xffffaaaa=0 ✗.)

## iptables on sun

pretest adds mangle rule to mark return traffic so it matches the outbound
XFRM policy (mark_out=3). Without this, bob's replies to alice won't be
encrypted (mark=0 won't match the policy with mark=3).

```
sun::iptables -t mangle -A PREROUTING -d PH_IP_ALICE -j MARK --set-mark 3
```

## Key evaltest.dat steps

```
# Initial SA with marks
sun::swanctl --list-sas --raw 2> /dev/null::mobike.*mark-in=00000002 mark-out=00000003::YES
alice::ping -c 1 PH_IP_BOB::64 bytes from PH_IP_BOB: icmp_.eq=1::YES

# Change NAT mapping on moon
moon::iptables -t nat -F
moon::iptables -t nat -A POSTROUTING -o eth0 -s 10.1.0.0/16 -p udp -j SNAT --to-source PH_IP_MOON:5000-5100
moon::conntrack -F

# ping -c 5 triggers MOBIKE via new NAT port; early packets lost during
# migration, later packets succeed once MOBIKE completes
alice::ping -c 5 PH_IP_BOB::64 bytes from PH_IP_BOB: icmp_.eq=1::YES
sun::cat /var/log/daemon.log::remote endpoint changed from PH_IP_MOON\[1...] to PH_IP_MOON\[5...]::YES

# All four marks must survive migration
sun::swanctl --list-sas --raw 2> /dev/null::mobike.*mark-in=00000002 mark-out=00000003::YES
sun::ip xfrm state::mark 0x2/0xffffaaaa::YES
sun::ip xfrm state::mark 0x3/0xffffbbbb::YES
sun::ip xfrm state::output-mark 0x6/0xffff6666::YES
sun::ip xfrm state::output-mark 0x5/0xffffdddd::YES

# Functional verification: traffic still flows (mangle rule still matches mark_out=3)
alice::ping -c 1 PH_IP_BOB::64 bytes from PH_IP_BOB: icmp_.eq=1::YES
```

## Open questions

1. Does your strongSwan MOBIKE implementation trigger XFRM_MSG_MIGRATE_STATE
   on alice's side too, or only on sun (responder)?
2. Should we also check ip xfrm state on alice's side?
3. Any specific log message to grep for confirming XFRM_MSG_MIGRATE_STATE
   was used (vs old xfrm_state_update path)?
