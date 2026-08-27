# Lunaframe Client

Domain language for Lunaframe Client: a desktop game-streaming client that connects to third-party host software. Glossary only — behavior and design live in `proposal.md` and `docs/adr/`.

## Language

### Machines and software

**Lunaframe Client**:
The desktop product itself — the software this repository builds.
_Avoid_: the app, Moonlight, Lunaframe (bare, when meaning the software)

**Client Device**:
One physical machine running one installation of Lunaframe Client; the unit that settings and profiles key on.
_Avoid_: client, device (bare)

**Host**:
A machine that runs streaming software and exposes Host Applications to Lunaframe Client.
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
One of up to four controller positions a Client Device presents to a Host within one Session; scoped to that Session, never shared across Clients.
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

**Wake Provider**:
An explicitly user-configured external service that sends Wake-on-LAN packets on a Host's behalf; wake success, Host readiness, and connection are three separate outcomes.
_Avoid_: WoL server

## Banned usage

- "profile" unqualified — always Streaming Profile or Controller Map.
- "app" unqualified — always Lunaframe Client or Host Application.
- "client" unqualified — always Lunaframe Client or Client Device.
