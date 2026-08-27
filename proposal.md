# Jochona Client

## Desktop Game Streaming Client Project Proposal

**Project:** Jochona  
**Deliverable:** Jochona Client  
**Document status:** Client-only scope — revised 2026-08-27 after source-verified review; binding decisions live in `CONTEXT.md` (glossary) and `docs/adr/`  
**Target platforms:** Windows, macOS, and Linux  
**Primary host target:** Vibepollo on Windows  
**Compatibility targets:** Apollo, Sunshine, and other GameStream-compatible hosts where practical  
**Network model:** LAN or user-managed private networking such as Tailscale  
**License assumption:** GPL-3.0-compatible derivative of Moonlight Qt  

Factual claims about upstream projects in this document were verified against upstream sources on 2026-08-27; the evidence file is `docs/research/moonlight-ecosystem-facts.md`.

---

## 1. Executive Summary

Jochona Client will be a modern, controller-first desktop game-streaming and remote-access client for Windows, macOS, and Linux. It will preserve Moonlight's mature low-latency streaming core while replacing the surrounding application experience with a cohesive, polished interface comparable in usability to contemporary commercial remote-play products.

The initial client will connect to existing Vibepollo, Apollo, and Sunshine hosts. It will support personal gaming VMs, home gaming systems, and hosts shared with trusted friends over Tailscale or another private network. It will not provision VMs, allocate GPUs, operate a central identity service, implement a Raspberry Pi control plane, or replace Vibepollo in this phase.

The product goal is broader than "Moonlight with a new skin," but narrower than an entire cloud-gaming platform. Jochona Client should provide an exceptional end-to-end experience from the client user's perspective:

1. Find or add an available host.
2. Wake it directly or request wake through a configured external service.
3. Pair securely.
4. Browse applications and desktops.
5. Select appropriate controller, display, audio, and network settings automatically.
6. Connect quickly and remain connected through ordinary device and network changes.
7. Diagnose problems in plain language.
8. Disconnect without accidentally terminating the host application.

The client must be fully usable with a controller while retaining first-class keyboard and mouse behavior for remote desktop use.

---

## 2. Scope Boundary

## 2.1 In scope

- A shared Windows, macOS, and Linux desktop client.
- Vibepollo-first integration with graceful Sunshine and Apollo compatibility.
- Controller-first navigation and complete keyboard/mouse parity.
- Modern controller discovery, visualization, calibration, remapping, assignment, and profiles.
- Game and desktop streaming.
- Modern theming and personalization.
- LAN discovery, manual host entry, pairing, and trusted-host management.
- Wake-on-LAN: extension of the mechanism already shipped in Moonlight Qt (see 6.5).
- A client-side interface for optional external wake providers.
- Tailscale-aware host connectivity without requiring a Jochona cloud service.
- Unified application library across multiple paired hosts.
- Per-host, per-client-device, per-display, and per-game profiles.
- Session overlay, reconnection, suspend/resume, and diagnostics.
- Modern codec, HDR, color, frame-rate, audio, and input configuration.
- Optional client capabilities that activate only when supported by the host.
- Packaging, updates, accessibility, localization readiness, and release engineering.

## 2.2 Explicitly out of scope

- Raspberry Pi Wake-on-LAN server implementation.
- Stream Deck annunciator or administrator panel implementation.
- Physical-server, hypervisor, VM, or GPU orchestration.
- GPU scheduling or multi-user session allocation.
- A central Jochona account, invitation, entitlement, or billing service.
- A mandatory cloud relay or discovery service.
- A new host streaming service or a Vibepollo fork.
- Changes to Vibepollo, Apollo, or Sunshine required solely to ship the first client release.
- macOS or Linux host implementation.
- iOS, iPadOS, Android, Android TV, or tvOS clients.
- General-purpose unrestricted USB-over-network functionality.
- Flathub submission (blocked by Flathub's generative-AI policy; see 6.15 and the risk register).

## 2.3 Future-compatible, not current deliverables

The client architecture should allow later integration with:

- A Raspberry Pi Wake-on-LAN and infrastructure-status service.
- A Stream Deck control panel.
- VM and GPU session brokers.
- A future Jochona Host or Vibepollo-derived host.
- Self-hosted identity and friend-access services.
- Optional rendezvous or media relays where direct connectivity is impossible.
- Cross-device configuration export/import (first post-1.0 feature; the storage schema must stay dump-clean to keep this a SELECT-and-zip operation).

These future systems must connect through documented adapters or negotiated capabilities. Their possible existence must not inflate the current client into a control-plane project.

---

## 3. Problem Statement

Moonlight's streaming core remains technically strong, but its surrounding desktop experience does not consistently feel like a modern console or polished remote-access product. Important behavior is distributed across client settings, host configuration, scripts, third-party utilities, operating-system controls, and tribal knowledge.

The principal client-side gaps are:

- A utilitarian interface with inconsistent controller-first behavior.
- Limited controller discovery, testing, assignment, remapping, and profile management (the underlying protocol features — motion, adaptive triggers, LED, battery — already exist upstream; the missing part is UI and configuration).
- No cohesive, safe theming system.
- Fragile handling of offline hosts, Wake-on-LAN, reconnection, and client sleep.
- Manual selection of display modes, codecs, frame rates, HDR, and bitrates.
- Limited per-game and per-device configuration.
- Weak multi-host library organization (upstream is single-active-host by construction).
- An insufficiently polished remote-desktop experience compared with Parsec.
- Technical statistics that do not tell the user what is wrong or how to fix it.
- Inconsistent behavior when moving among handheld screens, laptops, monitors, docks, ultrawides, and 4K televisions.
- Host-specific capabilities that are difficult to discover and expose coherently.

Jochona Client should solve these problems without rewriting the proven transport and decoder stack prematurely.

---

## 4. Product Principles

### 4.1 Excellent defaults, visible control

The client should make a strong automatic choice but always allow the user to inspect and override it.

### 4.2 Controller-first, not controller-only

Every ordinary game-streaming workflow must work with a controller. Remote desktop must still feel native with keyboard, mouse, trackpad, pen, and touch where supported.

### 4.3 Direct by default

Video, audio, and input should travel directly between Jochona Client and the selected host. Tailscale may provide network reachability, but Jochona should not require a central media relay.

### 4.4 Capability-driven compatibility

Features must appear when the paired host advertises or successfully probes them. The client must not assume behavior from a product name. Version-string gating is permitted only inside the Sunshine/GFE adapter, where version strings are the protocol itself; every other layer consumes the normalized capability model (7.4).

### 4.5 Recover rather than fail

A brief Wi-Fi interruption, controller reconnection, dock transition, or client sleep should trigger recovery whenever the host session remains viable.

### 4.6 Local ownership

Host credentials, profiles, artwork overrides, and settings belong to the user. No Jochona account or telemetry service is required.

---

## 5. Target Users and Scenarios

### 5.1 Private gaming-cloud owner

The user connects to multiple personal gaming VMs and physical PCs located at home or remotely. Hosts may use different GPUs, resolutions, capabilities, and network paths. Jochona remembers the correct profile for each client and host combination.

### 5.2 Trusted friend

A friend installs Jochona Client, receives private-network access and host pairing permission through mechanisms managed outside the client, and sees only the hosts and applications paired with that installation. Jochona provides a polished experience but does not own the underlying account or VM entitlement system in this phase.

### 5.3 Controller-first living-room player

The user launches Jochona on a television-connected device, wakes or selects a host, chooses a game, and controls the entire session without reaching for a keyboard.

### 5.4 Handheld player

The client runs on a ROG Ally, Steam Deck, or similar handheld. It recognizes integrated controls, chooses a native display profile, survives suspend/resume, and changes configuration when docked.

### 5.5 Laptop and remote-desktop user

The client runs on a MacBook, Windows laptop, or Linux laptop. It provides predictable keyboard shortcuts, direct or captured pointer modes, clipboard behavior, an on-screen keyboard, audio routing, and a responsive desktop experience.

### 5.6 Advanced user

The user can inspect and control codec choice, chroma mode, bitrate, latency stages, packet loss, frame pacing, controller transport, HDR, and host capabilities without exposing that complexity to ordinary users.

---

## 6. Functional Requirements

## 6.1 Modern application shell

The application shell is built in Qt Quick Controls 2 — the incumbent upstream technology — by replacing Moonlight's QML screens with Jochona screens incrementally behind feature flags (ADR-0001). "Desktop connections" is a filtered view over the library, not a separate entity: the desktop is a Host Application of kind `desktop`.

Primary screens:

- Welcome and first-run setup.
- Hosts.
- Unified library.
- Game or application details.
- Desktop connections (filtered library view).
- Controller manager.
- Profiles.
- Settings.
- Diagnostics.
- In-session overlay.

Interaction requirements:

- Deterministic D-pad and analog-stick focus movement.
- No unreachable, invisible, or ambiguous focus targets.
- Controller-driven host pairing and on-screen text entry.
- Full keyboard and mouse navigation.
- Correct Xbox, PlayStation, Nintendo, and Steam Deck glyphs, built on the vendored CC0 Kenney Input Prompts pack (`assets/vendor/`).
- Configurable confirm and cancel conventions.
- Clear long-press and secondary-action prompts.
- Adjustable repeat delay and speed.
- Fast startup and immediate restoration of the previous screen.

## 6.2 Controller system

The controller manager will provide:

- Stable device identity where the operating system permits it.
- Device name, vendor, connection type, battery, and capabilities.
- Live visualization of buttons, sticks, triggers, touchpads, and motion sensors. (Motion, adaptive-trigger, LED, and battery plumbing already exist end-to-end in moonlight-common-c; the manager adds visualization and configuration, and must reflect that host-side support varies.)
- Reliable hot-plug and reconnect behavior.
- Explicit Player Slot assignment and reordering. Player Slots are Session-scoped: the Client Device presents one pad per connected controller into the Host's pad pool, up to the protocol maximum of 16 (`MAX_GAMEPADS` in upstream `SdlInputHandler`); the four-player ordering UI is presentation, not a cap. Concurrent clients are governed by host policy and surfaced as the host's busy response. One active Session per Client Device.
- Stick and trigger calibration.
- Dead zones, anti-dead zones, sensitivity, and response curves.
- Button remapping. Controller Maps apply client-side in the input pipeline before protocol send, so they work with every host; each map carries a raw-passthrough toggle that bypasses all transforms for latency comparison and debugging.
- Rumble and trigger-rumble tests.
- Per-controller and per-game profiles.
- Configurable application shortcuts separate from game input.
- Detection and guidance for likely duplicate inputs from Steam Input or other remappers.

Controller transmission modes:

1. **Compatible:** present the device as an Xbox-compatible controller.
2. **Native type:** preserve the controller's family identity as the wire vocabulary permits — Xbox, PlayStation-family, Nintendo, or Steam (`LI_CTYPE_XBOX/PS/NINTENDO/STEAM`) — plus every semantic capability the per-controller capability bitmask carries (motion, touchpads, trigger rumble, LED, battery). The protocol cannot currently distinguish DualSense from DualShock (`LI_CTYPE_PS` collapses PS3/PS4/PS5); a type-refinement proposal to moonlight-common-c is a tracked follow-up, and no UI may promise finer identity than the wire carries.
3. **Advanced device:** reserved for future explicitly approved HID or specialty-controller transport.

Advanced device mode is not required for the initial release and must never silently expose arbitrary USB devices.

## 6.3 Theming and personalization

Built-in themes:

- System.
- Light.
- Dark.
- OLED black.
- High contrast.

User customization:

- Accent color.
- Static background image or gradient.
- Card shape, opacity, and density.
- Font scale.
- Artwork density and aspect ratio.
- Animation intensity and reduced-motion mode.
- Optional per-host accent or artwork.

Custom themes will be versioned, data-only packages containing a manifest, design tokens, and static assets. Themes may not contain QML, JavaScript, native libraries, shell commands, or other executable code. Invalid themes must fail safely to a built-in theme.

## 6.4 Host discovery and pairing

- Automatic LAN discovery (upstream already browses `_nvstream._tcp.local.` via mDNS; retain and extend).
- Manual hostname, IPv4, IPv6, or Tailscale address entry.
- Friendly host names and optional artwork.
- Online, waking, connecting, busy, paired, unpaired, and unavailable states.
- Pairing flow usable entirely with a controller.
- Trusted-host management and unpairing.
- Pairing credential storage using OS security facilities (upstream keeps the pairing certificate and private key inside plain QSettings — moving them into the OS vault is real, new client work).
- A previously Trusted Host whose identity changes is hard-blocked: streaming to that host is refused until the user explicitly re-pairs. No soft "proceed anyway" path in v1.
- On first run, an opt-in importer offers to adopt hosts, cached MACs, and the pairing identity (certificate and key) from an existing official Moonlight installation, migrating the key from QSettings into the OS vault. Import is always offered, never automatic.
- Connection history that does not expose secrets in logs or UI.

## 6.5 Wake-on-LAN and external wake providers

### Direct local Wake-on-LAN

Baseline status: Moonlight Qt already sends magic packets (102-byte frame to all known host addresses on ports 9 and 47009 plus the GFE dynamic port set) using the MAC cached from the host's `/serverinfo` `<mac>` element, exposed as a one-click `wakeComputer()` action. Jochona inherits this mechanism; the new work is everything around it.

Known protocol gap: Sunshine publishes `<mac>` only over HTTPS to paired clients, derived from the interface the client reached — a client paired over Tailscale caches the wrong (TUN) MAC for LAN wake, and unpaired requests receive a `00:00:00:00:00:00` placeholder.

While a host is online, the client may cache:

- MAC address.
- Last known addresses.
- Interface and subnet information.
- Broadcast candidates.
- Optional SecureOn data stored in the OS credential store.

Host details provide manual overrides that beat the auto-cache: MAC address, WoL port, and broadcast candidates, plus a "re-probe from LAN" action.

When an offline host is selected, the client will:

1. Send redundant magic packets over relevant local interfaces.
2. Enter a visible waking state.
3. Probe for the host using bounded backoff.
4. Continue automatically when the host becomes available.
5. Offer retry and diagnostics if waking fails.

### External wake provider interface

Remote Wake-on-LAN generally requires an always-on device on the destination LAN (verified: Tailscale does not forward layer-2 magic packets). Jochona Client will define a small adapter boundary for an optional external wake service, such as the user's planned Raspberry Pi service.

The initial client may support a configurable authenticated HTTPS endpoint or deep link, but implementation of that server is out of scope. Requirements for any adapter:

- Explicit user configuration.
- Secure credential storage.
- TLS validation.
- Bounded timeouts and retries.
- No assumption that wake success means the streaming host is ready.
- Separate wake, readiness, and connection states.
- No unauthenticated public requests.

The GL.iNet Comet remains an external emergency-management mechanism and is not a Jochona dependency.

## 6.6 Tailscale-aware connectivity

Jochona does not embed or administer Tailscale in the initial release. It should work cleanly when Tailscale is already installed and connected. Upstream already classifies reachability as LAN/VPN/internet through interface heuristics (virtual-interface names, MTU below 1500, MAC prefix, on-link tests); Jochona builds on that rather than duplicating it:

- Accept stable hostnames and private addresses.
- Distinguish LAN and private-overlay routes where practical, with explicit Tailscale labeling rather than generic "VPN."
- Avoid treating private-overlay addresses as unsafe public hosts.
- Preserve pairing when the network path changes.
- Detect loss and restoration of reachability.
- Offer useful diagnostics without modifying tailnet policy.
- Never require friends to share a Jochona account.

Tailscale identity, node sharing, ACLs, and connectivity remain external administrative concerns.

## 6.7 Unified application library

- Merge applications from multiple paired hosts.
- Search, favorites, recently played, collections, and hidden entries.
- Deduplicate the same game across hosts: normalized titles *suggest* a Library Entry grouping; a user always confirms, merges, or splits. Automatic merging without user confirmation is prohibited, and no host's entry is ever silently hidden.
- Use host-provided artwork and metadata when available.
- Permit local artwork and metadata overrides.
- Indicate which paired hosts expose an application.
- Allow automatic preferred-host selection or manual selection.
- Store per-game streaming and controller profiles.
- Distinguish games, desktops, launchers, and utilities through a kind field on one entity type, not parallel types.
- Cache enough metadata for a useful offline host card without exposing protected data.

## 6.8 Display and streaming profiles

Streaming Profiles may be selected based on:

- Client Device.
- Active display.
- Docked or undocked state.
- Resolution, aspect ratio, scale, and orientation.
- Refresh rate and VRR capability.
- HDR and color-space capability.
- Decoder capability.
- Network path and measured quality.
- Host and selected application.

Selection rule: the most-specific matching Streaming Profile wins — profiles constraining more dimensions rank higher; ties resolve in the fixed order Host Application > Client Device > Display > Host > Global. The overlay must always be able to explain which profile matched and why. A Profile Pin binds one Streaming Profile to a context and bypasses selection entirely.

Example profiles:

- Handheld native 60 FPS.
- Handheld native 120 FPS.
- Living-room 4K120 HDR.
- Laptop balanced.
- Remote constrained network.
- Desktop 4:4:4 productivity.

The client should request the desired mode and allow Vibepollo or another capable host to manage its virtual display. On Apollo and Vibepollo this is a *request* via launch parameters and host-side virtual-display drivers (`virtualDisplay`, `scaleFactor`, `hdrMode`, `clientVrrRequested`, `output_name_override`), not a negotiated handshake; no GameStream endpoint lists display modes. The client must not add competing host-display scripts.

## 6.9 Remote desktop experience

Jochona should approach Parsec-like usability without attempting to reproduce Parsec's proprietary service.

Client-side requirements:

- Reliable absolute and relative pointer modes.
- Pointer capture and release that is obvious and reversible.
- High-resolution scrolling.
- System shortcut forwarding with configurable safety controls.
- On-screen keyboard and safe special-key menu.
- Multi-monitor selection when the host exposes it (present on Apollo/Vibepollo via `/serverdisplaymodes` and `/displaydevice`; absent on Sunshine).
- Windowed and borderless-fullscreen presentation. Exclusive (flip-model) fullscreen is deliberately excluded (ADR-0003): the overlay must render independently of the streamed frame, which exclusive modes cannot guarantee.
- Clipboard synchronization when supported by the host — verified available on Apollo and Vibepollo (`/actions/clipboard`, permission-gated), not available on Sunshine.
- File transfer only through an explicitly negotiated and permissioned future capability.
- Clear privacy indicators for microphone, clipboard, and peripheral access.
- Distinct game and desktop presets.

Clipboard, multi-monitor, microphone, and other reverse channels must appear only when the host supports them. Protocol-level microphone support is unverified on all current host software and must be probe-gated, not assumed.

## 6.10 Session overlay

The controller-accessible overlay will provide:

- Resume.
- Disconnect client.
- End host application when permitted.
- Reconnect or restart the stream without ending the application.
- Controller assignment and profile switching.
- Audio output and microphone controls.
- Resolution, frame-rate, HDR, codec, chroma, and bitrate controls.
- Network and latency status.
- On-screen keyboard and special keys.
- Configurable shortcuts that do not conflict with the game.

Summon default: hold Guide for 750 ms (a bare Guide press is reserved by the Windows Game Bar) or Win+G in desktop contexts; both remappable. The overlay must render independently of the streamed frame and remain available when decoding stalls.

## 6.11 Reconnection and lifecycle recovery

- Automatic reconnection after short network interruptions.
- Recovery after Wi-Fi roaming or interface changes.
- Resume after client sleep and wake.
- Restoration of controller slots and profiles.
- Restoration of HDR, pointer capture, and audio routing.
- Clear indication of whether the host application remains active.
- Bounded retry with configurable timeout.
- Explicit resume, restart stream, end application, and return-to-library actions.

## 6.12 Adaptive streaming

The first release will preserve dependable manual controls and add a guided connection test. Later client releases may adapt based on:

- Packet loss.
- Jitter.
- Round-trip time.
- Decoder queue depth.
- Render lateness.
- Network-interface transitions.

Vibepollo hosts expose a runtime bitrate endpoint (`GET /bitrate?kbps=`) and a capability advertisement (`/api/abr/capabilities`) intended exactly for client-driven adaptation; the client-side ABR controller belongs to Jochona, and host support varies by adapter.

Recommended degradation order:

1. Reduce bitrate.
2. Adjust quality parameters supported by the host.
3. Change from 4:4:4 to 4:2:0.
4. Reduce frame rate.
5. Reduce resolution as a last resort.

Adaptive behavior must avoid constant visible oscillation and permit user-defined quality floors.

## 6.13 Audio and microphone

Audio requirements:

- Stereo and multichannel output.
- Client audio-device selection.
- Safe recovery when an audio device disconnects.
- Session-level volume and mute.
- Clear Bluetooth headset mode warnings.

Microphone transport is desirable but requires host support. When negotiated, the client should provide:

- Low-latency microphone capture.
- Push-to-talk and mute.
- Input selection and level monitoring.
- Visible privacy indication.
- Optional local noise suppression and gain control.

## 6.14 Diagnostics

Jochona will expose raw metrics and plain-language interpretations:

- Host encode latency.
- Network latency, loss, and jitter.
- Client decode latency.
- Render and presentation latency.
- Frames dropped by network, decoder, and renderer.
- Codec, bit depth, chroma mode, HDR, resolution, and frame rate.
- Controller connection and polling status.

Examples of actionable explanations:

- Host encoder is the current bottleneck.
- Client decoder cannot sustain the selected AV1 mode.
- Wi-Fi jitter is causing dropped frames.
- Host and client refresh rates are mismatched.
- Frames arrive on time but are presented late.
- Bluetooth controller latency appears unusually high.

The client should generate a redacted diagnostic bundle suitable for support requests.

## 6.15 Updates, release channels, and distribution

Channels and packaging:

- Stable and preview/nightly channels.
- Windows: SignPath-signed installers (free for qualifying open-source projects; requires the repository to be public), x86-64 with ARM64 retained — upstream CI already cross-builds both.
- macOS: Developer ID-signed and notarized application and disk image (Apple Developer Program membership held).
- Linux: Flatpak built by CI and served from project infrastructure — an OSTree repository hosted on GitHub Pages as the update channel, with a `.flatpakref` and a `.flatpak` bundle attached to GitHub Releases (see `docs/github-setup.md`). **Flathub submission is not viable**: Flathub's generative-AI policy rejects applications containing AI-generated or AI-assisted code, documentation, or other content, and forbids AI-generated submission material.
- No silent downgrade.

Updates:

- Check-and-notify only: the client queries the GitHub Releases API, compares semantic versions, and links to the release page. No in-app install on any platform in v1. Linux installs via Flatpak update through their own bundle source.

Localization readiness:

- v1 ships English only. Every UI string must pass through the Qt Linguist extraction pipeline from Milestone 1, enforced by CI; community translations are a post-1.0 concern.

Configuration migration:

- Single SQLite database with a schema-migration runner; pre-migration backups use `VACUUM INTO`, which produces a consistent snapshot while WAL is active (a raw file copy can capture a database mid-checkpoint and is not WAL-safe). Rollback swaps the snapshot in on the next clean start.

---

## 7. Technical Architecture

## 7.1 Technology choices

| Area | Initial choice | Rationale |
| --- | --- | --- |
| Application framework | Qt 6.11 with Qt Quick Controls 2, replaced screen-by-screen in place | This is Moonlight Qt's *incumbent* stack (verified against upstream `app.pro` and CI), so the shell work is new QML screens replacing old ones, not a framework migration (ADR-0001). Qt-6-only drops upstream's still-supported Qt 5.12 Linux path deliberately. |
| Core language | C++ | Matches Moonlight Qt and platform/video integrations. |
| Streaming protocol | `moonlight-common-c` | Mature GameStream implementation; kept near-upstream with deliberate bumps. |
| Controller input | Existing SDL2 path initially behind a new abstraction | Preserve compatibility before considering SDL3. |
| Video and audio | Existing Moonlight renderers, FFmpeg, libplacebo, and platform hardware APIs | Avoid destabilizing the most mature subsystem. |
| Settings and profiles | One SQLite database with a schema-migration runner | Settings, profiles, library cache, and history in one file; rollback is a `VACUUM INTO` snapshot swap; secrets never live here. |
| Secrets | OS credential stores | Upstream stores pairing certificate and private key in plain QSettings — migrating them to Keychain/DPAPI/libsecret is new client work, along with provider tokens. |
| Build system | Preserve upstream (qmake) initially; evaluate CMake after parity | Avoid coupling product work to a foundational migration. |

Electron, Tauri, Flutter, or a browser-based shell are not recommended for the primary streaming window. Qt already supports the target platforms and integrates with native low-latency video, HDR, raw input, and controller handling.

## 7.2 Repository structure

```text
jochona-client/
  app/                  Startup and dependency composition
  core/                 Shared models, events, profiles, and session state
  streaming/            Moonlight session integration
  input/                Controllers, keyboard, mouse, touch, and shortcuts
  library/              Hosts, applications, artwork, and collections
  integrations/
    gamestream/          Baseline protocol behavior
    sunshine/            Sunshine capability adapter
    apollo/              Apollo capability adapter
    vibepollo/           Vibepollo capability adapter
    wake/                Optional external wake-provider adapters
  platform/
    windows/
    macos/
    linux/
  qml/                   Shell, reusable controls, themes, and overlay
  assets/                Vendored glyph packs and brand sources (see assets/vendor/README.md)
  tests/
  packaging/
```

## 7.3 Important interfaces

```cpp
class IStreamingSession;
class IHostAdapter;
class IHostDiscovery;
class IControllerBackend;
class IControllerTransport;
class IProfileStore;
class IThemeManager;
class IWakeProvider;
class ICredentialStore;
class IDisplayService;
class INetworkDiagnostics;
```

The QML layer will consume stable models and commands rather than directly manipulating SDL, FFmpeg, sockets, or host-specific APIs.

## 7.4 Host capability negotiation

Feature-level capability detection is required, built on verified ground truth about what hosts actually expose:

- **Baseline Sunshine/GFE:** no capability registry exists. Clients see version strings, `ServerCodecModeSupport` and `MaxLumaPixelsHEVC`, per-application `IsHDRSupported`, RTSP SDP feature-flag exchange, and accepted launch parameters. Avoiding version-string assumptions entirely is unachievable here; version gating is therefore confined to the Sunshine adapter as a documented exception.
- **Apollo:** richer presence-probed fields (`VirtualDisplayCapable`, `VirtualDisplayDriverReady`, `scaleFactor`, a per-client `Permission` bitmask) and extra endpoints (`/actions/clipboard`, `/serverdisplaymodes`, `/action/bitrates`). No self-identifying field — probing is the only detection mechanism.
- **Vibepollo:** the only declarative capability endpoint in the ecosystem (`GET /api/abr/capabilities` → versioned feature list) plus the runtime `/bitrate` endpoint and `VirtualDisplay*` flags published even pre-pairing.

Each adapter normalizes what it finds into one internal capability model; UI features render from that model, never from a product name. A conceptual internal representation may resemble:

```json
{
  "host_type": "vibepollo",
  "capabilities": {
    "virtual_display": true,
    "client_mode_matching": true,
    "clipboard": true,
    "multi_monitor": false,
    "microphone_input": false,
    "native_controller_type": true,
    "controller_motion": true,
    "hdr": true,
    "av1": true
  }
}
```

Each optional feature must fail independently and fall back to baseline GameStream behavior where possible. New host-side work belongs in a separate project even if Jochona Client defines the corresponding extension contract.

## 7.5 State model

Three explicit models replace any single linear chain, because pairing happens once per host while sessions recur:

**Host Availability** (per Host, observed):

```text
UNKNOWN → ONLINE / OFFLINE → WAKING → ONLINE / UNREACHABLE
```

**Trust** (per Host, changed only by user or security events):

```text
UNPAIRED → PAIRING → TRUSTED  (any state → IDENTITY_CHANGED → re-pair or forget)
```

**Session** (per connection attempt):

```text
IDLE → CONNECTING → STREAMING → RECONNECTING → STREAMING
                     STREAMING → ENDING → IDLE
                     any → FAULTED → IDLE
```

UI state, diagnostics, and permitted actions must derive from these explicit models rather than scattered booleans. "Is the host application still running?" is derived from Session plus host-reported state, never inferred from transport liveness.

---

## 8. Platform Support

| Platform | Initial target | Important platform work |
| --- | --- | --- |
| Windows | Windows 11 x64; ARM64 built by inherited upstream CI, community-tested rather than release-gated | D3D/DXVA, HDR, raw input, DPAPI, installer signing, handheld testing. |
| macOS | macOS 14+ on Apple Silicon; Intel Macs community-tested, not release-gated | VideoToolbox, Metal, HDR/EDR, Keychain, notarization, controller permissions. |
| Linux | Current x86-64 distributions through Flatpak served from project Releases | VAAPI/Vulkan, Wayland and X11, libsecret, portals, packaging variance. |
| Steam Deck/Bazzite | First-class Linux configuration | Gamescope, integrated controls, suspend/resume, docking, external displays, and HDR maturity; read-only OS means home-directory persistence only. |

Platform parity is defined by user outcomes rather than identical implementation.

---

## 9. Security and Privacy

- No mandatory Jochona account or cloud dependency.
- Pairing credentials and wake-provider tokens stored in OS credential facilities (new work; upstream keeps them in plain settings).
- Host identity changes hard-block streaming until deliberate re-pairing.
- Themes are data-only and cannot execute code.
- External wake providers are explicitly configured and authenticated.
- Advanced peripheral transport is disabled by default and allowlisted if later implemented.
- Host actions respect server-side permissions.
- Destructive actions require confirmation.
- Logs redact credentials, tokens, private keys, clipboard contents, and sensitive identifiers.
- Telemetry is absent by default. Any future telemetry must be opt-in and documented.
- Tailscale provides connectivity, not implicit Jochona authorization.

---

## 10. Accessibility

- Complete keyboard navigation.
- Complete controller navigation.
- High-contrast themes.
- Adjustable interface scale.
- Reduced-motion mode.
- Status indicators that do not rely on color alone.
- Screen-reader-friendly labels where supported.
- Remappable application shortcuts.
- Configurable hold durations and input-repeat behavior.

---

## 11. Testing Strategy

## 11.1 Automated testing

- Unit tests for profiles, settings migration, capability negotiation, themes, Wake-on-LAN packets, and reconnection logic.
- Contract tests using captured or simulated GameStream, Sunshine, Apollo, and Vibepollo responses — fixtures from the verified route and field sets in `docs/research/moonlight-ecosystem-facts.md`.
- Controller mapping tests using virtual devices where practical.
- QML navigation tests proving that every primary action is controller-reachable.
- Snapshot tests for built-in themes and representative display sizes.
- Malformed-input tests for themes, host metadata, artwork, and capability responses.
- CI builds for Windows, macOS, and Linux on every merge (inheriting and preserving upstream's workflow matrix).

## 11.2 Hardware validation

- Windows desktop or handheld client.
- macOS Apple Silicon client.
- Linux Wayland client.
- Linux X11 client while supported.
- ROG Ally on Windows.
- Steam Deck or ROG Ally on Bazzite/Gamescope.
- 4K120 HDR display.
- Xbox controller.
- DualSense controller.
- Switch Pro controller.
- Wired and Bluetooth input paths.
- Ethernet, strong Wi-Fi, impaired Wi-Fi, and Tailscale scenarios.
- Vibepollo, Apollo, and Sunshine hosts where supported.

## 11.3 Performance gates

Changes must not regress:

- Decode latency.
- Render latency.
- Input latency.
- Frame pacing.
- CPU and GPU use.
- Memory growth during long sessions.
- Controller polling behavior.
- Stream startup and reconnection time.

---

## 12. Delivery Roadmap

**Version 1.0 is Milestones 0–3.** Milestones 4 and 5 ship as post-1.0 point releases.

## Milestone 0: Compatibility fork

**Objective:** Establish an independently branded client that streams successfully on all three desktop platforms.

Deliverables:

- Import Moonlight Qt's full history by bare-mirror push into `Jochona/jochona-client` and add upstream as a sync remote (ADR-0002).
- Adopt separate application identifiers and configuration directories.
- Keep upstream's CI workflows green through the rebrand (they already cover Windows x64/ARM64, macOS, AppImage, and Steam Link).
- Pair with Vibepollo and stream one application.
- Validate H.264, HEVC, AV1, audio, keyboard, mouse, and a standard controller.
- Establish baseline performance measurements.

Exit criteria:

- All three platforms can add a host, pair, launch, stream, and disconnect.
- Jochona can coexist with official Moonlight without overwriting its settings.

## Milestone 1: Modern shell and themes

Deliverables:

- New host, library, application, settings, and pairing QML screens replacing the Moonlight screens incrementally behind feature flags.
- Deterministic controller focus.
- Controller glyph system.
- Jochona visual identity and design tokens.
- Built-in themes and safe theme-package schema.
- Favorites, recent applications, search, and host-state indicators.
- Extension of inherited Wake-on-LAN: the visible waking state, manual MAC/port/broadcast overrides, and re-probe.
- Qt Linguist string-extraction pipeline enforced in CI.

Exit criteria:

- A new user can pair, select an application, stream, and disconnect without a keyboard or mouse.

## Milestone 2: Controller system

Deliverables:

- Controller manager and live visualization.
- Calibration, dead zones, remapping, and player ordering.
- Per-controller and per-game profiles.
- Reliable hot-plug behavior.
- Compatible and native-type transmission modes within the wire's family vocabulary, plus raw-passthrough toggle.
- Duplicate-input diagnostics.

Exit criteria:

- Xbox, DualSense, and Switch Pro controllers pass the wired and Bluetooth test matrix on supported platforms.

## Milestone 3: Session resilience and remote desktop

Deliverables:

- Controller-driven session overlay.
- Automatic reconnection.
- Suspend/resume recovery.
- Network-interface transition handling.
- Reliable pointer capture and desktop controls.
- Guided connection test and diagnostics.
- Tailscale path validation.

Exit criteria:

- Brief Wi-Fi interruption, client suspend, controller reconnection, or LAN-to-Tailscale path change does not unnecessarily terminate the host application.

## Milestone 4 (post-1.0): Host-aware enhancements

Deliverables:

- Vibepollo, Apollo, and Sunshine capability adapters against the normalized capability model.
- Display-mode and virtual-display visibility as request surfaces (launch parameters, not handshakes).
- Multi-monitor selection where supported.
- Clipboard and microphone surfaces that appear only when negotiated.
- Improved per-host and per-game profiles.
- External wake-provider client interface.

Exit criteria:

- Optional features activate correctly without breaking baseline Sunshine compatibility.

## Milestone 5 (post-1.0): Adaptive quality and advanced input

Potential deliverables:

- Adaptive bitrate and quality controls, client-driven against Vibepollo's runtime bitrate endpoint.
- Better VRR and frame-pacing behavior.
- Broader host coverage and presentation for the adaptive-trigger and motion support that already exists in the protocol.
- Carefully scoped specialty-controller support.
- Additional remote-desktop capabilities negotiated with future hosts.

---

## 13. Initial Engineering Backlog

1. ~~Confirm project name availability~~ Done 2026-08-27: "Lunaframe" was contested (GitHub handle taken; trademark and package-registry status unverifiable in time), so the product is **Jochona**. The `Jochona` GitHub org is registered and owned; repository-name collisions were zero. Remaining: purchase the `jochona` domain (drives the application id) and sweep npm/PyPI/crates.io/app-store handles.
2. Import Moonlight Qt history by bare-mirror push and establish the upstream sync remote per ADR-0002.
3. Set the application identifier: `app.jochona.client`, contingent on domain purchase; until then M0 builds carry a provisional `dev.jochona.client` and the id is frozen before M1 branding.
4. Separate Jochona configuration from official Moonlight; implement the opt-in Moonlight settings importer (hosts, MACs, pairing key into the OS vault).
5. Produce reproducible Windows, macOS, and Linux development builds.
6. Add a Vibepollo compatibility smoke test (pin the exact tested Vibepollo version; it releases fast).
7. Record baseline latency, frame pacing, and controller behavior.
8. Inventory dependencies and platform-specific technical debt.
9. Introduce host, controller, theme, wake-provider, credential, and display interfaces.
10. Implement a new QML home screen behind a feature flag.
11. Implement controller focus and glyph primitives on the vendored Kenney Input Prompts pack.
12. Implement theme tokens and two proof themes.
13. Extend inherited Wake-on-LAN with the waking-state machine and manual overrides.
14. Add redacted structured logging and diagnostic export.
15. Establish performance gates before deeper refactoring.

The first cycle intentionally excludes a host fork, Raspberry Pi service, Stream Deck plugin, account service, and VM broker.

---

## 14. Risks and Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Fork diverges from Moonlight | Security and compatibility updates become expensive | Full-history import keeps upstream mergeable (ADR-0002); keep `moonlight-common-c` close to upstream, minimize core edits, and sync weekly. |
| Vibepollo changes rapidly or stalls | Optional integration breaks | Verified lineage is Sunshine → Apollo → Vibepollo; Vibepollo ships Windows-x64-only and trails Apollo, which is itself semi-dormant. Isolate Vibepollo behavior, negotiate individual capabilities, and never let an adapter's absence break baseline. |
| Cross-platform behavior differs | Unreliable releases | Define behavioral acceptance tests and maintain physical-device testing. |
| Controller changes regress latency or mappings | Core product failure | Preserve the current path initially, instrument it, and gate changes against tests. |
| Themes harm readability or performance | Poor UX | Use validated tokens, constrained assets, safe fallbacks, and overlay-specific limits. |
| Parsec-like ambitions trigger a premature host rewrite | Client never ships | Keep all initial milestones compatible with existing hosts and move host work to a separate proposal. |
| External wake integration becomes insecure | Remote-control exposure | Require explicit HTTPS configuration, scoped credentials, and no default public endpoint. |
| Large simultaneous refactors stall delivery | No usable release | Complete the compatibility fork and vertical slice before migrating dependencies. |
| GPL or trademark obligations are mishandled | Distribution problems | Retain notices, publish source, use distinct branding, and perform formal name review. Full-history import preserves upstream attribution. |
| Flathub's generative-AI policy excludes the project from Linux store discovery | Reduced Linux reach; manual update flows | Verified and accepted: Flatpak ships from the project's own GitHub Releases; revisit only if the policy or exception path changes. |
| Release signing outages (SignPath queue, Apple notarization) | Delayed releases | Channel check-and-notify design means a skipped release is late, not broken; document a manual-signing fallback for security releases. |

---

## 15. Licensing and Upstream Relationship

Moonlight Qt is licensed under GPL-3.0. A distributed derivative should remain GPL-3.0-compatible, retain required notices, and provide corresponding source. Jochona must use a distinct name, application identifier, visual identity, and release channel so users do not mistake it for an official Moonlight release. The bare-mirror import preserves upstream commit history, which serves attribution.

General bug fixes, platform fixes, tests, and isolated protocol improvements should be considered for upstream contribution — including the controller-type refinement noted in 6.2. Jochona-specific UI, themes, profiles, and integrations may remain in the fork.

---

## 16. Success Criteria

Jochona Client succeeds when:

- Windows, macOS, and Linux users receive comparable core functionality.
- A controller-only user can perform every ordinary game-streaming workflow.
- Keyboard and mouse remote desktop feels deliberate rather than incidental.
- Local Wake-on-LAN works as an integrated wake-and-connect experience, including hosts whose auto-cached MAC is wrong.
- An external Raspberry Pi wake service can be added through a narrow client adapter without becoming part of the client codebase.
- Controllers can be identified, tested, ordered, calibrated, remapped, and profiled.
- Themes provide meaningful personalization without executable extension code.
- Vibepollo users receive deeper integration while Sunshine compatibility remains intact.
- Transient network and device events no longer routinely terminate play.
- Diagnostics explain likely causes and remedies.
- Tailscale-connected hosts behave like first-class private hosts.
- Streaming performance is at least equivalent to the Moonlight baseline.
- Linux users can install and update via Flatpak from project-controlled infrastructure without Flathub dependence.

---

## 17. Recommended Immediate Next Step

Begin with a focused compatibility phase:

1. Purchase the `jochona` domain; freeze `app.jochona.client` and sweep remaining package-registry handles.
2. Import Moonlight Qt history by bare-mirror push and establish the upstream remote.
3. Build and run the unmodified client on Windows, macOS, and Linux using the inherited CI matrix.
4. Confirm end-to-end streaming against the intended Vibepollo version.
5. Capture baseline performance and controller behavior.
6. Add architectural interfaces without changing runtime behavior.
7. Replace the home screen with a minimal controller-navigable QML screen behind a feature flag.
8. Demonstrate inherited Wake-on-LAN with the new waking state followed by automatic connection, including one manual-MAC-overridden host.
9. Validate a manually configured Tailscale host, confirming pairing survives a LAN-to-overlay path change.

The phase should end with a distributable client prototype, not a host service or orchestration platform.

---

## 18. Initial Brand Assets

The initial visual exploration uses a luminous video frame, crescent-shaped orbital signal, and cyan-to-violet transport trail over a near-black and deep navy base.

**Status note:** these concepts were created under the abandoned Lunaframe identity (moon-and-frame imagery). They are placeholders for a Jochona identity pass, not approved direction.

| Asset | Working filename | Intended use |
| --- | --- | --- |
| Application icon concept | `jochona-app-icon-concept-v1.png` | Launcher icon, repository avatar, and early builds. |
| Wide hero concept | `jochona-steam-hero-concept-v1.png` | Steam/Steam Deck hero, repository banner, and landing page. |
| Portrait grid concept | `jochona-steam-grid-concept-v1.png` | Steam library portrait capsule. |

These are concept assets rather than production masters. The selected mark should be redrawn as deterministic SVG and exported into exact Windows, macOS, Linux, Steam, and accessibility variants.

---

## 19. Reference Projects

- Moonlight Qt: <https://github.com/moonlight-stream/moonlight-qt>
- Moonlight common C library: <https://github.com/moonlight-stream/moonlight-common-c>
- Vibepollo: <https://github.com/Nonary/Vibepollo>
- Apollo: <https://github.com/ClassicOldSong/Apollo>
- Sunshine: <https://github.com/LizardByte/Sunshine>
- Tailscale documentation: <https://tailscale.com/kb/>
- Kenney Input Prompts (CC0 glyph source): <https://kenney.nl/assets/input-prompts>
- Flathub requirements including the generative-AI policy: <https://docs.flathub.org/docs/for-app-authors/requirements>
- Upstream fact-check evidence: `docs/research/moonlight-ecosystem-facts.md`
