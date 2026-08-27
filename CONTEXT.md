# Jochona Client

Domain language for Jochona Client: a desktop game-streaming client that connects to third-party host software. Glossary only — behavior and design live in `proposal.md` and `docs/adr/`.

## Language

### Machines and software

**Jochona Client**:
The desktop product itself — the software this repository builds.
_Avoid_: the app, Moonlight, Jochona (bare, when meaning the software)

**Client Device**:
One physical machine running one installation of Jochona Client; the unit that settings and profiles key on.
_Avoid_: client, device (bare)

**Host**:
A machine that runs streaming software and exposes Host Applications to Jochona Client.
_Avoid_: server, PC, box

**Host Software**:
The streaming service running on a Host — Vibepollo, Apollo, or Sunshine.
_Avoid_: host (when meaning the software rather than the machine)

### Launching

**Host Application**:
A title a Host exposes for launch — a game, a launcher, a utility, or the desktop.
_Avoid_: app, game (when including desktops/utilities)

**Desktop**:
The Host's interactive desktop, exposed as a Host Application of kind `desktop`; never a separate entity kind.
_Avoid_: remote session, live desktop

**Session**:
One connection between one Client Device and one running Host Application; has a start and an end.
_Avoid_: stream, connection (bare)

**Player Slot**:
One of up to sixteen protocol-level controller positions (upstream `MAX_GAMEPADS`) a Client Device presents to a Host within one Session, one pad per connected controller; scoped to that Session, never shared across Clients.
_Avoid_: player number (host-global), controller index

### Trust and availability

**Trust**:
The per-Host relationship — Unpaired, Pairing, Trusted, or Identity Changed. Established once per Host; never a per-Session phase.
_Avoid_: paired (as a session state)

**Host Availability**:
The observed liveness of a Host — Unknown, Online, Offline, Waking, or Unreachable.
_Avoid_: online/offline (as session states)

### Configuration


**Streaming Profile**:
A named bundle of display, codec, and network settings, selected by Client Device, display, Host, and Host Application.
_Avoid_: profile (bare), quality preset

**Profile Pin**:
A user's explicit binding of one Streaming Profile to a context, bypassing profile selection.
_Avoid_: lock, default profile

**Controller Map**:
A bundle of button remaps, calibration, and response curves, scoped per controller and optionally per Host Application.
_Avoid_: controller profile, mapping (bare), profile (bare)

**Theme**:
A data-only package of design tokens and static assets; may contain no executable content.
_Avoid_: skin

### Library

**Library Entry**:
The user-confirmed grouping of Host Applications across Hosts that represent the same game; title normalization only suggests the grouping, a user confirms, merges, or splits it.
_Avoid_: dedupe, merged app

**Wake**:
Bringing a powered-off Host to life so it can accept Sessions. Three shapes: **Direct Wake** — the Client Device itself sends the Wake-on-LAN magic packet; **Overlay Wake** — a network overlay that emulates LAN broadcast (ZeroTier-class) delivers it; **Relay Wake** — a Jochona-operated always-on Relay sends it on the Client Device's behalf. Only Direct Wake is supported today; Tailscale is deliberately not a Wake path (it does not carry layer-2 broadcast).
_Avoid_: remote wake (ambiguous), WoL server

**Wake Provider**:
A Wake mechanism other than the Client Device's own Direct Wake — Overlay Wake or Relay Wake. Configured explicitly per Host; wake success, Host readiness, and connection remain three separate outcomes.
**Controller Surface**:
A touch-sensitive region on a controller that carries input beyond buttons — trackpads (Steam Controller/Steam Deck) or touchpads (DualShock 4, DualSense); bindable as a pointer source via a Controller Map transform.
_Avoid_: touchpad (as a catch-all), pad (ambiguous with Player Slot)

## Banned usage

- "profile" unqualified — always Streaming Profile or Controller Map.
- "app" unqualified — always Jochona Client or Host Application.
- "client" unqualified — always Jochona Client or Client Device.
