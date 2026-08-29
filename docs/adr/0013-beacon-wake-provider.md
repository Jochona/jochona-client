---
status: accepted
---

# Ship Beacon Wake as an explicit Wake Provider

ADR-0004 called an always-on LAN helper "Relay Wake." That term now conflicts with Constellation Relay, which forwards an encrypted streaming Session.

Jochona Client instead names Beacon Wake as a durable per-Host provider. The Client sends one authenticated request, and Beacon owns the 0/1/3-second packet burst.

Provider failures never trigger silent fallback, and Beacon identity changes require re-pairing. This decision supersedes ADR-0004's Relay Wake name, but not its Direct Wake constraints.

The wire contract is [Beacon Client Protocol v1](https://github.com/Jochona/jochona-beacon/blob/main/docs/protocols/client-v1.md).
