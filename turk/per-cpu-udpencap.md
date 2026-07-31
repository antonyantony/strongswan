# MUSE (draft-antony-ipsecme-muse) in strongSwan

Source draft: `/home/a/git/ietf-drafts/draft-antony-ipsecme-udp-encap/publish/draft-antony-ipsecme-udp-encap-multiport-00.txt`

MUSE ("Multiple UDP Source ports for ESP in UDP Encapsulation") extends
RFC 9611 per-resource Child SAs with a **negotiated, deterministic**
Ephemeral Source Port per per-resource Child SA, instead of RFC 9611's
plain per-CPU behavior of a random, unnegotiated outbound port
(`per_cpu_sas = encap`) with the peer accepting any inbound source port.
A deterministic, negotiated port lets both ends steer traffic for a given
resource onto the same NIC RX queue (RSS/ECMP) reliably, instead of
relying on the ESP SPI alone.

## Mechanism

- **Capability negotiation**: both peers unconditionally announce
  `UDP_EPHEMERAL_SOURCE_PORT` in IKE_AUTH; each side enables
  `EXT_UDP_EPHEMERAL_PORT` on the IKE_SA when it sees the peer's notify.
  If either side doesn't support it, behavior falls back to plain
  `per_cpu_sas = encap` (random, unnegotiated ports) — no protocol errors.
- **No explicit port signaling.** TBD1 carries no data, and
  `SA_RESOURCE_INFO` only carries the CPU id. The initiator picks its
  Ephemeral Source Port and sends the per-resource `CREATE_CHILD_SA`
  request *from* it; the responder learns the port purely from the
  packet's actual UDP source port.
- **Asymmetric.** Only the initiator of a given per-resource Child SA gets
  a dedicated Ephemeral Source Port; the responder keeps using its normal
  port 4500 for that SA's traffic in both directions. For the benefit to
  apply symmetrically, each peer separately initiates its own per-resource
  Child SAs for its own local per-CPU flows — which side ends up as
  initiator (and so gets the Ephemeral Source Port) for any given
  per-resource Child SA isn't deterministic when both peers are
  distributing traffic independently.
- **No Linux kernel changes needed.** Every piece of kernel-facing
  plumbing (encap port selection via `xfrm_encap_tmpl`, per-CPU SAs via
  `XFRMA_SA_PCPU`) already exists as a generic, existing XFRM netlink API.
  This is a userspace-only (charon) feature.

## Implementation

- **Notify type / extension bit / config flag**
  (`src/libcharon/encoding/payloads/notify_payload.h/.c`,
  `src/libcharon/sa/ike_sa.h`, `src/libcharon/config/child_cfg.h`):
  `UDP_EPHEMERAL_SOURCE_PORT = 40970` (private-use range, RFC 7296
  §3.10.1 — swap for the real IANA-assigned value once TBD1 is resolved),
  `EXT_UDP_EPHEMERAL_PORT` ike extension bit, `OPT_UDP_EPHEMERAL_SOURCE_PORT`
  child config flag.
- **IKE_AUTH capability announcement** (`sa/ikev2/tasks/ike_auth.c`):
  inline in `build_i()`/`process_r()`/`build_r()`/`process_i()`, mirroring
  the existing `MULTIPLE_AUTH_SUPPORTED`/`IKEV2_MESSAGE_ID_SYNC_SUPPORTED`
  pattern — no dedicated task, since this is a one-shot no-data
  capability bit with no stateful post-setup protocol.
- **Ephemeral port allocation and sending** (`sa/ikev2/tasks/child_create.c`):
  `build_i()` gates on `!rekey && child.per_cpu &&` `CREATE_CHILD_SA &&`
  `EXT_UDP_EPHEMERAL_PORT &&` `OPT_UDP_EPHEMERAL_SOURCE_PORT`; on match,
  `allocate_ephemeral_port()` picks a port (49152–65535, avoiding this
  IKE_SA's other active per-resource ports — host-wide coordination across
  *different* peers/IKE_SAs isn't attempted), then `build_i()` calls
  `message->set_source()` directly to override the outgoing packet's
  source port (the same pattern `ike_mobike.c` uses for address updates).
  `process_r()` unconditionally captures the request's actual source port;
  `handle_per_resource()` decides whether to actually use it (extension +
  config gate).
- **Port placement on the installed SA** (`sa/child_sa.c`
  `install_internal()`): per-direction, per-role (initiator/responder ×
  inbound/outbound) port placement — see the code for the concrete matrix.
- **Fallback SA follows the IKE_SA's own port.** `child_create.c` sets
  `child.per_cpu = true` for every non-rekey Child SA on a
  `per_cpu_sas`-enabled connection, including the Fallback SA created
  during IKE_AUTH before any per-resource negotiation happens — nothing
  distinguished it from a genuine per-resource SA (`this->cpu ==
  CPU_ID_MAX` is the only marker). Verified live against unmodified
  upstream strongSwan + kernel (no MUSE code at all) that this pre-existing
  gap (Tobias Brunner's 2021 `per_cpu_sas = encap`, `child_sa.c`) also
  randomizes the Fallback SA's own outbound port instead of leaving it at
  4500. Fixed for MUSE specifically (gated on
  `OPT_UDP_EPHEMERAL_SOURCE_PORT`, deliberately not touching plain
  `encap`'s existing behavior): `install_internal()` now skips both the
  wildcard-accept (inbound) and random-port (outbound) branches when
  `this->cpu == CPU_ID_MAX`, so under `muse` the Fallback SA's `ip xfrm
  state` correctly shows `encap type espinudp sport 4500 dport 4500` in
  both directions.
- **NAT-remapping guard** (`sa/ikev2/task_manager_v2.c`): the existing
  implicit `update_hosts()` call (used for non-MOBIKE NAT remapping) is
  guarded to skip `CREATE_CHILD_SA` when `EXT_UDP_EPHEMERAL_PORT` is
  negotiated — otherwise it would adopt a per-resource request's ephemeral
  source port as the IKE_SA's tracked peer port whenever `COND_NAT_THERE`
  is set (the common NATed-initiator case), silently violating §6.3 item 3
  ("MUST NOT interpret as IKE_SA roaming").
- **Socket backend: requires `socket_dynamic`**, not `socket_default`.
  Sending a control message from an arbitrary local port needs
  `socket_dynamic_socket.c`'s on-demand `(family, port)`-keyed socket
  table (`find_socket()`/`sender()`); `socket_default` only has a fixed
  set of sockets. `socket_dynamic_socket.c` also proactively binds the
  standard ports (500/4500) at startup and only enables `UDP_ENCAP` on
  the NAT-T (4500) socket, not on port 500 — both are prerequisites for a
  responder's very first `IKE_SA_INIT` to work correctly with
  `socket-default` disabled (enabling `UDP_ENCAP` on a plain port 500
  socket makes the kernel misinterpret an inbound plaintext IKE packet as
  ESP-in-UDP and silently drop it).

## Configuration

`connections.<conn>.children.<child>.per_cpu_sas = muse` (`swanctl.opt`,
`vici_config.c`'s `parse_opt_cpus`) — behaves like `encap` (random,
unnegotiated per-CPU ports), but additionally negotiates the deterministic
Ephemeral Source Port described above. Falls back to plain `encap`
behavior if the peer doesn't support the extension.

## Known limitations / not implemented

- **Path validation before outbound install** (draft §6.5): not
  implemented — both inbound and outbound install immediately, same as
  plain `per_cpu_sas = encap` today. `register_outbound()`/
  `install_outbound()` (`child_sa.h`/`.c`, already used for rekey
  collision handling) would handle the "never blackhole" half, but
  promoting a per-resource SA from "inbound-only, deferred outbound" to
  "both installed" once reachability is confirmed needs a new
  kernel-interface signal (a distinguishable per-SA packet-count trigger,
  decoupled from the existing rekey-soft-limit `expire` semantics and
  from `trap_manager_t`'s acquire handling, which only fits brand-new
  trap entries) — real new plumbing, not a reuse of anything existing.
- **NAT keepalives per active port pair** (§8.1, §8.2): not implemented —
  the existing NAT-T keepalive job only covers the Fallback SA's
  4500↔4500 pair.
- **Abandoned-SA recovery over NAT** (§7.4) and **`TS_MAX_QUEUE`
  rejection** (§6.2): not implemented — `TS_MAX_QUEUE` is defined but
  never sent (`child_create.c`'s per-resource-SA-limit path is a stub).
- **NIC steering** (§6.6, `ethtool --config-ntuple`): not implemented;
  would be a standalone updown-style helper script, not core charon code.
- **fd lifecycle**: `socket_dynamic` keeps a bound port's socket open for
  the life of the process, not tied to per-resource Child SA teardown —
  §6.4's "MUST NOT process further IKEv2 messages [on that port] after
  negotiation" isn't separately enforced.
- **Host-wide ephemeral-port coordination** across different peers/IKE_SAs
  isn't attempted — `allocate_ephemeral_port()` only avoids collisions
  within the same IKE_SA.

## Testing

Unit-tested at the notify/extension/option level (`make check`, all
`libstrongswan`/`libcharon`/`exchange` suites). Live two-peer end-to-end
verification runs against a separate Nix-based test framework
(`/home/a/dresden/nix-ipsec-kernel-strongswan/nix/testing/tests/ikev2-muse/net-2-net-ipv4.nix`
— moon/sun gateways + alice/bob endpoints, `per_cpu_sas = muse`, 4 vCPUs so
per-CPU distribution has something to distribute across), run via
`nix-build -A checks.tests.muse.net-2-net-ipv4`. Confirms: IKE_SA_INIT →
IKE_AUTH → CREATE_CHILD_SA all complete, `UDP_EPHEMERAL_SOURCE_PORT`
negotiated both directions, a real per-resource Child SA installs with a
non-4500 dport on exactly the initiating side (`ip xfrm state` — `encap
type espinudp` with `dport` in 49152-65535), and traffic actually flows
end-to-end through the tunnel.

No in-tree (`testing/tests/ikev2/*`) UML scenario exists yet for this
feature — the existing `per-cpu-sas-encap` UML test covers plain
`per_cpu_sas = encap` only.
