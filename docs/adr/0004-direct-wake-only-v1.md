---
status: superseded by ADR-0013
---

# Direct Wake-on-LAN only for v1; overlay and relay wake deferred

Waking a powered-off Host requires delivering a magic packet to a layer-2 broadcast domain the Host listens on. A Client Device on the same LAN can do this directly; nothing else can — Tailscale and WireGuard-class overlays route layer-3 unicast only and drop the broadcast, and a Host behind arbitrary NAT is unreachable from the open Internet. We decided v1 ships only Direct Wake (the inherited Moonlight mechanism, refined), explicitly excluding Tailscale as a Wake path, and deferring the two mechanisms that could cross those boundaries.

## Decision

- **v1: Direct Wake only.** The Client Device sends the magic packet itself. The UI must state the shared-network requirement where wake is offered, so a Tailscale user learns the constraint at the point of failure instead of filing a bug.
- **Future: Overlay Wake** — overlays that emulate LAN broadcast (ZeroTier-class) can carry Direct Wake unchanged; the client's job becomes "send via this interface/route", a per-Host wake egress setting, not a new protocol.
- **Eventual: Relay Wake** — a Jochona-operated always-on Relay per user LAN (or a hosted relay plus a lightweight Host-side listener) sends the packet when the Client Device cannot. This is the endgame for true remote wake and pairs with future NAT-traversal relay streaming.
- **Excluded by design: Tailscale as Wake transport.** It cannot carry the packet; no client-side cleverness fixes this. Cache hygiene still matters: pairing over Tailscale caches the TUN interface MAC from Sunshine's `<mac>`, which breaks even same-LAN Direct Wake — the manual MAC override (proposal §6.5) is the escape hatch.

## Consequences

- Proposal §6.5's "client-side interface for optional external wake providers" moves from v1 scope to future scope; the Raspberry Pi wake service remains a personal side project until an adapter boundary has a second implementation (Overlay or Relay) worth abstracting.
- The `Wake Provider` vocabulary is retained: Direct / Overlay / Relay are the three shapes; any wake UI and the eventual `IWakeProvider` boundary speak these terms.
- If Relay Wake lands, wake, Host readiness, and connection remain three separate states — the relay answering "packet sent" is never "Host ready".
