# Jochona Client

Domain language for Jochona Client: a desktop game-streaming client that connects to third-party host software. Glossary only — behavior and design live in `proposal.md` and `docs/adr/`.

## Language

### Machines and software

**Jochona Client**:
The desktop product itself — the software this repository builds.
_Avoid_: the app, Moonlight, Jochona (bare, when meaning the software)

**Client Device**:
One physical installation of Jochona Client, identified by a generated stable ID and optional user name rather than hardware fingerprinting; settings and synchronization key on it.
_Avoid_: client, device (bare), hardware ID

**Host**:
A machine that runs streaming software and exposes Host Applications to Jochona Client.
_Avoid_: server, PC, box

**Host Software**:
The streaming service running on a Host — Jochona Host, Vibepollo, Apollo, or Sunshine.
_Avoid_: host (when meaning the software rather than the machine)

**Jochona Host**:
The first-party, Sunshine-derived Host Software in a separate repository; it preserves baseline GameStream and exposes authenticated versioned Jochona extensions.
_Avoid_: Vibepollo replacement, companion, server

**Virtual Display Adapter**:
The separately signed, versioned Windows display driver/adapter used by Jochona Host for headless and VM display modes; physical-display hosting does not require it.
_Avoid_: virtual display (bare), dummy monitor

**Virtual Display Pool**:
The administrator-defined named virtual-display modes managed by Jochona Host; a Session acquires one, applies its requested mode/HDR, and restores/releases it without driver churn.
_Avoid_: virtual monitor list, per-session display

**Host Policy**:
Administrator ceilings and permissions constraining which proven Encoder Tuples and controls a Client may request; the Client still chooses Effective Settings within them.
_Avoid_: Host profile, Host settings (bare)

**Encoder Tuple**:
One exact capture/display/codec/profile/bit-depth/chroma/resolution/FPS/HDR combination advertised only after vendor query and real probe-frame encoding succeed.
_Avoid_: codec support, GPU supports AV1

**Host Identity**:
The combination of a Host-provided unique ID and its pinned certificate; an unexpected change to either enters Trust state Identity Changed.
_Avoid_: hostname, address, certificate (alone)

**Host–Client Pair**:
One Host as reached from one Client Device; the scope for settings that should follow that exact machine pairing but not every device or Host Application.
_Avoid_: host profile, client-host profile, pair (bare)

**Controller Identity**:
The Client Device's stable reference to one physical controller, using an OS/SDL path when reliable and a user-disambiguated minted ID otherwise.
_Avoid_: controller path, GUID (bare), device index

**Active Controller**:
The controller that most recently produced input; its family owns global button prompts.
_Avoid_: primary controller, first controller

**Credential Vault**:
The operating-system secret store used for pairing credentials; when unavailable, credentials may live only for the current process and are never persisted in plaintext.
_Avoid_: credential file, secret database, key store (bare)

### Launching

**Host Application**:
A title a Host exposes for launch — a game, a launcher, a utility, or the desktop.
_Avoid_: app, game (when including desktops/utilities)

**Desktop**:
The Host's interactive desktop, exposed as a Host Application of kind `desktop`; never a separate entity kind.
_Avoid_: remote session, live desktop


**Host Capacity**:
Whether a Host is Ready or Busy for a new Session; the first Jochona Host release permits one active Session and reports the current Host Application without exposing credentials.
_Avoid_: availability (liveness), occupancy, session count

**Session**:
One connection between one Client Device and one running Host Application; has a start and an end.
_Avoid_: stream, connection (bare)

**Player Slot**:
One of up to sixteen protocol-level controller positions (upstream `MAX_GAMEPADS`) a Client Device presents to a Host within one Session, one pad per connected controller; scoped to that Session, never shared across Clients.
_Avoid_: player number (host-global), controller index

**Player Slot Order**:
The Client Device's persisted controller ordering used to assign Session-scoped Player Slots when a Session starts or a controller reconnects.
_Avoid_: player numbers, advisory slots

### Trust and availability

**Trust**:
The per-Host relationship — Unpaired, Pairing, Trusted, or Identity Changed. Established once per Host; never a per-Session phase.
_Avoid_: paired (as a session state)

**Host Availability**:
The observed liveness of a Host — Unknown, Online, Offline, Waking, or Unreachable.
_Avoid_: online/offline (as session states)

### Configuration


**Streaming Profile**:
A reusable named baseline bundle of display, codec, network, audio, and input settings selected for a Client Device and Display Context before sparse Settings Patches apply.
_Avoid_: profile (bare), quality preset, snapshot

**Settings Baseline**:
The installation-wide starting values inherited by every Session context on one Client Device.
_Avoid_: global profile, defaults (when meaning persisted user choices)

**Display Context**:
One locally identified display situation on a Client Device — physical display fingerprint plus dock state, optionally user-named — used to select a Streaming Profile.
_Avoid_: monitor profile, screen preset, dock profile

**Settings Patch**:
A sparse set of explicitly changed fields attached to a Host–Client Pair, Library Entry, or Host Application; omitted fields inherit, and specificity is Pair → Library Entry → Host Application.
_Avoid_: override profile, snapshot, profile (bare)

**Session Patch**:
A sparse, non-persistent settings layer for one Session; restart-class fields queue until Apply & Reconnect, and Save to… explicitly copies chosen fields into a durable Settings Patch.
_Avoid_: temporary profile, live profile, unsaved settings

**Effective Settings**:
The resolved values and per-field provenance for one Session context after Settings Baseline, context selection, Settings Patches, Profile Pins, Launch Adaptation, and final capability safety limits.
_Avoid_: current settings, merged profile, Negotiator result

**Launch Adaptation**:
Pre-Session selection and safety adjustment using Client Device, display, Host, Host Application, and connection-test facts; it never changes a running Session.
_Avoid_: adaptive bitrate, ABR, auto bitrate

**Runtime ABR**:
Post-1.0, bitrate-only adjustment during an active Session through a capability-selected Runtime Bitrate Sink; Vibepollo is the known adapter and unsupported Hosts expose no actuator.
_Avoid_: autoAdjustBitrate, adaptive (bare), reconnect adaptation

**Profile Pin**:
A user's explicit field or Streaming Profile choice that bypasses automatic selection and Launch Adaptation; impossible values may still be reduced by capability safety limits with an explanation.
_Avoid_: lock, hard requirement, default profile

**Quality Floor**:
A user's minimum acceptable field value; if capability safety requires a lower value, Jochona asks before launch rather than silently violating it or permanently blocking access.
_Avoid_: minimum profile, hard pin

**Session Volume**:
Client-local gain applied to one Session; always available and distinct from changing the Host operating system.
_Avoid_: volume (bare), client volume

**Host Volume**:
The Host's own system/output level, exposed only when Host Software capability and permission exist.
_Avoid_: volume (bare), remote volume

**Controller Map**:
A sparse controller-wide bundle of remaps, calibration, and curves, narrowed by Library Entry and then Host Application when present.
_Avoid_: controller profile, mapping (bare), profile (bare)

**Support Bundle**:
A deliberate, previewed export of redacted logs, Effective Settings provenance, capabilities, route state, and recent failures; Host addresses are excluded unless the user opts in for that export.
_Avoid_: log dump, diagnostics zip, telemetry

**Clipboard Permission**:
A Host–Client Pair grant for send, receive, or bidirectional clipboard transfer; disabled by default and never synchronized in the background.
_Avoid_: clipboard sync (bare), shared clipboard

**Release Channel**:
The signed update-notification feed selected by the user — Stable by default or Preview by explicit opt-in; it never installs or downgrades Jochona silently.
_Avoid_: update ring, beta flag

**Theme**:
A data-only package of design tokens and static assets; may contain no executable content.
_Avoid_: skin

### Library

**Library Entry**:
The user-confirmed grouping of Host Applications across Hosts that represent the same game; it owns shared title, category, artwork, favorites, recents, and Library-level Settings Patches, while one Host Application may narrow them.
_Avoid_: dedupe, merged app, title match

**Local History**:
Bounded Client Device records of launches, Session outcomes, route quality, and diagnostic summaries retained for 90 days by default; users may shorten, extend, disable, or clear it.
_Avoid_: telemetry, analytics, logs (when meaning structured history)

**Host Choice Pin**:
A user's persistent choice of one eligible Host for a Library Entry, bypassing automatic Host scoring until cleared or ineligible.
_Avoid_: preferred host (ambiguous), last host

**Beacon**:
An owner-paired, always-on device or service on a Host's LAN. It observes Host presence and can originate an authorized Beacon Wake, but never launch or control.
_Avoid_: Constellation Relay, wake server, hub, agent

**Wake**:
The action that brings a powered-off Host online so it can accept Sessions.
_Avoid_: Host readiness, connection, remote wake (ambiguous)

**Direct Wake**:
A Wake in which the Client Device sends the Wake-on-LAN packet burst on its current network.
_Avoid_: local wake, client wake

**Beacon Wake**:
A Wake in which Jochona Client sends one authorized request and a paired Beacon originates the packet burst on the Host's LAN.
_Avoid_: Relay Wake, remote Wake, proxy Wake

**Overlay Wake**:
A deferred Wake in which a network overlay that carries LAN broadcast delivers the packet.
_Avoid_: Tailscale Wake, Beacon Wake

**Wake Provider**:
The explicit, durable per-Host choice between Direct Wake and a paired Beacon Wake. A failed provider never switches to another provider silently.
_Avoid_: fallback chain, Relay Wake, automatic route

**Controller Surface**:
A touch-sensitive region on a controller that carries input beyond buttons — trackpads (Steam Controller/Steam Deck) or touchpads (DualShock 4, DualSense); bindable as a pointer source via a Controller Map transform.
_Avoid_: touchpad (as a catch-all), pad (ambiguous with Player Slot)

### Shell

**Shell**:
Everything the user sees outside a Session — the persistent chrome and screens of Jochona Client.
_Avoid_: UI, frontend, home screen (that is one Screen)

**Screen**:
One full-view surface of the Shell (Home, Host Detail, Settings); Screens form a stack with one on top.
_Avoid_: page, view, tab

**Route Bar**:
The named root destinations — Resume, Rigs, Library, Controllers, and Settings — arranged on one visible route.
_Avoid_: Nav Rail, sidebar, tab bar

**Resume Stage**:
The first focused control on Home; it shows the next action and the Client Device → Host → Host Application route.
_Avoid_: Hero, banner, carousel

**Route Segment**:
A horizontal group of same-kind destinations connected by the visible focus path.
_Avoid_: Shelf, row, carousel

**Context Panel**:
A bounded overlay for a short secondary action that preserves the Screen beneath it; text entry and multi-step work use a Screen instead.
_Avoid_: Quick Sheet, dialog, popup

**Route Focus**:
The single outline, cue lamp, and short route trace identifying the one active target.
_Avoid_: Moonlit Focus, glow, selection ring


## Banned usage

- "profile" unqualified — always Streaming Profile or Controller Map.
- "app" unqualified — always Jochona Client or Host Application.
- "client" unqualified — always Jochona Client or Client Device.
