# Lunaframe Client

## Desktop Game Streaming Client Project Proposal

**Project:** Lunaframe  
**Deliverable:** Lunaframe Client  
**Document status:** Client-only scope  
**Target platforms:** Windows, macOS, and Linux  
**Primary host target:** Vibepollo on Windows  
**Compatibility targets:** Apollo, Sunshine, and other GameStream-compatible hosts where practical  
**Network model:** LAN or user-managed private networking such as Tailscale  
**License assumption:** GPL-3.0-compatible derivative of Moonlight Qt  

---

## 1. Executive Summary

Lunaframe Client will be a modern, controller-first desktop game-streaming and remote-access client for Windows, macOS, and Linux. It will preserve Moonlight's mature low-latency streaming core while replacing the surrounding application experience with a cohesive, polished interface comparable in usability to contemporary commercial remote-play products.

The initial client will connect to existing Vibepollo, Apollo, and Sunshine hosts. It will support personal gaming VMs, home gaming systems, and hosts shared with trusted friends over Tailscale or another private network. It will not provision VMs, allocate GPUs, operate a central identity service, implement a Raspberry Pi control plane, or replace Vibepollo in this phase.

The product goal is broader than “Moonlight with a new skin,” but narrower than an entire cloud-gaming platform. Lunaframe Client should provide an exceptional end-to-end experience from the client user's perspective:

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
- Direct local Wake-on-LAN.
- A client-side interface for optional external wake providers.
- Tailscale-aware host connectivity without requiring a Lunaframe cloud service.
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
- A central Lunaframe account, invitation, entitlement, or billing service.
- A mandatory cloud relay or discovery service.
- A new host streaming service or a Vibepollo fork.
- Changes to Vibepollo, Apollo, or Sunshine required solely to ship the first client release.
- macOS or Linux host implementation.
- iOS, iPadOS, Android, Android TV, or tvOS clients.
- General-purpose unrestricted USB-over-network functionality.

## 2.3 Future-compatible, not current deliverables

The client architecture should allow later integration with:

- A Raspberry Pi Wake-on-LAN and infrastructure-status service.
- A Stream Deck control panel.
- VM and GPU session brokers.
- A future Lunaframe Host or Vibepollo-derived host.
- Self-hosted identity and friend-access services.
- Optional rendezvous or media relays where direct connectivity is impossible.

These future systems must connect through documented adapters or negotiated capabilities. Their possible existence must not inflate the current client into a control-plane project.

---

## 3. Problem Statement

Moonlight's streaming core remains technically strong, but its surrounding desktop experience does not consistently feel like a modern console or polished remote-access product. Important behavior is distributed across client settings, host configuration, scripts, third-party utilities, operating-system controls, and tribal knowledge.

The principal client-side gaps are:

- A utilitarian interface with inconsistent controller-first behavior.
- Limited controller discovery, testing, assignment, remapping, and profile management.
- No cohesive, safe theming system.
- Fragile handling of offline hosts, Wake-on-LAN, reconnection, and client sleep.
- Manual selection of display modes, codecs, frame rates, HDR, and bitrates.
- Limited per-game and per-device configuration.
- Weak multi-host library organization.
- An insufficiently polished remote-desktop experience compared with Parsec.
- Technical statistics that do not tell the user what is wrong or how to fix it.
- Inconsistent behavior when moving among handheld screens, laptops, monitors, docks, ultrawides, and 4K televisions.
- Host-specific capabilities that are difficult to discover and expose coherently.

Lunaframe Client should solve these problems without rewriting the proven transport and decoder stack prematurely.

---

## 4. Product Principles

### 4.1 Excellent defaults, visible control

The client should make a strong automatic choice but always allow the user to inspect and override it.

### 4.2 Controller-first, not controller-only

Every ordinary game-streaming workflow must work with a controller. Remote desktop must still feel native with keyboard, mouse, trackpad, pen, and touch where supported.

### 4.3 Direct by default

Video, audio, and input should travel directly between Lunaframe Client and the selected host. Tailscale may provide network reachability, but Lunaframe should not require a central media relay.

### 4.4 Capability-driven compatibility

Features must appear when the paired host advertises or successfully probes them. The client must not assume behavior solely from a product name or version number.

### 4.5 Recover rather than fail

A brief Wi-Fi interruption, controller reconnection, dock transition, or client sleep should trigger recovery whenever the host session remains viable.

### 4.6 Local ownership

Host credentials, profiles, artwork overrides, and settings belong to the user. No Lunaframe account or telemetry service is required.

---

## 5. Target Users and Scenarios

### 5.1 Private gaming-cloud owner

The user connects to multiple personal gaming VMs and physical PCs located at home or remotely. Hosts may use different GPUs, resolutions, capabilities, and network paths. Lunaframe remembers the correct profile for each client and host combination.

### 5.2 Trusted friend

A friend installs Lunaframe Client, receives private-network access and host pairing permission through mechanisms managed outside the client, and sees only the hosts and applications paired with that installation. Lunaframe provides a polished experience but does not own the underlying account or VM entitlement system in this phase.

### 5.3 Controller-first living-room player

The user launches Lunaframe on a television-connected device, wakes or selects a host, chooses a game, and controls the entire session without reaching for a keyboard.

### 5.4 Handheld player

The client runs on a ROG Ally, Steam Deck, or similar handheld. It recognizes integrated controls, chooses a native display profile, survives suspend/resume, and changes configuration when docked.

### 5.5 Laptop and remote-desktop user

The client runs on a MacBook, Windows laptop, or Linux laptop. It provides predictable keyboard shortcuts, direct or captured pointer modes, clipboard behavior, an on-screen keyboard, audio routing, and a responsive desktop experience.

### 5.6 Advanced user

The user can inspect and control codec choice, chroma mode, bitrate, latency stages, packet loss, frame pacing, controller transport, HDR, and host capabilities without exposing that complexity to ordinary users.

---

## 6. Functional Requirements

## 6.1 Modern application shell

Primary screens:

- Welcome and first-run setup.
- Hosts.
- Unified library.
- Game or application details.
- Desktop connections.
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
- Correct Xbox, PlayStation, and Nintendo glyphs.
- Configurable confirm and cancel conventions.
- Clear long-press and secondary-action prompts.
- Adjustable repeat delay and speed.
- Fast startup and immediate restoration of the previous screen.

## 6.2 Controller system

The controller manager will provide:

- Stable device identity where the operating system permits it.
- Device name, vendor, connection type, battery, and capabilities.
- Live visualization of buttons, sticks, triggers, touchpads, and motion sensors.
- Reliable hot-plug and reconnect behavior.
- Explicit player-slot assignment and reordering.
- Stick and trigger calibration.
- Dead zones, anti-dead zones, sensitivity, and response curves.
- Button remapping.
- Rumble and trigger-rumble tests.
- Per-controller and per-game profiles.
- Configurable application shortcuts separate from game input.
- Detection and guidance for likely duplicate inputs from Steam Input or other remappers.

Controller transmission modes:

1. **Compatible:** present the device as an Xbox-compatible controller.
2. **Native type:** preserve Xbox, DualSense, DualShock, or Switch identity and supported semantic capabilities.
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

- Automatic LAN discovery.
- Manual hostname, IPv4, IPv6, or Tailscale address entry.
- Friendly host names and optional artwork.
- Online, waking, connecting, busy, paired, unpaired, and unavailable states.
- Pairing flow usable entirely with a controller.
- Trusted-host management and unpairing.
- Pairing credential storage using OS security facilities.
- Clear warning when a previously paired host identity changes unexpectedly.
- Connection history that does not expose secrets in logs or UI.

## 6.5 Wake-on-LAN and external wake providers

### Direct local Wake-on-LAN

While a host is online, the client may cache:

- MAC address.
- Last known addresses.
- Interface and subnet information.
- Broadcast candidates.
- Optional SecureOn data stored in the OS credential store.

When an offline host is selected, the client will:

1. Send redundant magic packets over relevant local interfaces.
2. Enter a visible waking state.
3. Probe for the host using bounded backoff.
4. Continue automatically when the host becomes available.
5. Offer retry and diagnostics if waking fails.

### External wake provider interface

Remote Wake-on-LAN generally requires an always-on device on the destination LAN. Lunaframe Client will define a small adapter boundary for an optional external wake service, such as the user's planned Raspberry Pi service.

The initial client may support a configurable authenticated HTTPS endpoint or deep link, but implementation of that server is out of scope. Requirements for any adapter:

- Explicit user configuration.
- Secure credential storage.
- TLS validation.
- Bounded timeouts and retries.
- No assumption that wake success means the streaming host is ready.
- Separate wake, readiness, and connection states.
- No unauthenticated public requests.

The GL.iNet Comet remains an external emergency-management mechanism and is not a Lunaframe dependency.

## 6.6 Tailscale-aware connectivity

Lunaframe does not embed or administer Tailscale in the initial release. It should work cleanly when Tailscale is already installed and connected:

- Accept stable hostnames and private addresses.
- Distinguish LAN and private-overlay routes where practical.
- Avoid treating private-overlay addresses as unsafe public hosts.
- Preserve pairing when the network path changes.
- Detect loss and restoration of reachability.
- Offer useful diagnostics without modifying tailnet policy.
- Never require friends to share a Lunaframe account.

Tailscale identity, node sharing, ACLs, and connectivity remain external administrative concerns.

## 6.7 Unified application library

- Merge applications from multiple paired hosts.
- Search, favorites, recently played, collections, and hidden entries.
- Deduplicate the same game while retaining host choices.
- Use host-provided artwork and metadata when available.
- Permit local artwork and metadata overrides.
- Indicate which paired hosts expose an application.
- Allow automatic preferred-host selection or manual selection.
- Store per-game streaming and controller profiles.
- Distinguish games, desktops, launchers, and utilities.
- Cache enough metadata for a useful offline host card without exposing protected data.

## 6.8 Display and streaming profiles

Profiles may be selected based on:

- Client device.
- Active display.
- Docked or undocked state.
- Resolution, aspect ratio, scale, and orientation.
- Refresh rate and VRR capability.
- HDR and color-space capability.
- Decoder capability.
- Network path and measured quality.
- Host and selected application.

Example profiles:

- Handheld native 60 FPS.
- Handheld native 120 FPS.
- Living-room 4K120 HDR.
- Laptop balanced.
- Remote constrained network.
- Desktop 4:4:4 productivity.

The client should request the desired mode and allow Vibepollo or another capable host to manage its virtual display. It must not add competing host-display scripts.

## 6.9 Remote desktop experience

Lunaframe should approach Parsec-like usability without attempting to reproduce Parsec's proprietary service.

Client-side requirements:

- Reliable absolute and relative pointer modes.
- Pointer capture and release that is obvious and reversible.
- High-resolution scrolling.
- System shortcut forwarding with configurable safety controls.
- On-screen keyboard and safe special-key menu.
- Multi-monitor selection when the host exposes it.
- Windowed, borderless, and exclusive/fullscreen presentation modes.
- Clipboard synchronization when supported by the host.
- File transfer only through an explicitly negotiated and permissioned future capability.
- Clear privacy indicators for microphone, clipboard, and peripheral access.
- Distinct game and desktop presets.

Clipboard, multi-monitor, microphone, and other reverse channels must appear only when the host supports them.

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

The overlay must render independently of the streamed frame and remain available when decoding stalls.

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

Lunaframe will expose raw metrics and plain-language interpretations:

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

## 6.15 Updates and release channels

- Stable and preview/nightly channels.
- Signed Windows installers and binaries.
- Signed and notarized macOS application and disk image.
- Flatpak-first Linux distribution plus AppImage where practical.
- Visible release notes and compatibility warnings.
- No silent downgrade.
- Configuration migration with rollback-safe backups.

---

## 7. Technical Architecture

## 7.1 Technology choices

| Area | Initial choice | Rationale |
| --- | --- | --- |
| Application framework | Qt 6 and Qt Quick/QML | Existing Moonlight architecture, native desktop support, controller-friendly UI, and direct rendering integration. |
| Core language | C++ | Matches Moonlight Qt and platform/video integrations. |
| Streaming protocol | `moonlight-common-c` | Mature GameStream implementation. |
| Controller input | Existing SDL2 path initially behind a new abstraction | Preserve compatibility before considering SDL3. |
| Video and audio | Existing Moonlight renderers, FFmpeg, and platform hardware APIs | Avoid destabilizing the most mature subsystem. |
| Settings and profiles | Versioned SQLite or schema-versioned local data | Supports migrations and multi-dimensional profiles. |
| Secrets | OS credential stores | Avoid plaintext pairing material and provider tokens. |
| Build system | Preserve upstream initially; evaluate CMake after parity | Avoid coupling product work to a foundational migration. |

Electron, Tauri, Flutter, or a browser-based shell are not recommended for the primary streaming window. Qt already supports the target platforms and integrates with native low-latency video, HDR, raw input, and controller handling.

## 7.2 Repository structure

```text
lunaframe-client/
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

Feature-level capability detection is required. A conceptual internal representation may resemble:

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

Each optional feature must fail independently and fall back to baseline GameStream behavior where possible. New host-side work belongs in a separate project even if Lunaframe Client defines the corresponding extension contract.

## 7.5 Session state machine

```text
DISCOVERED
  → OFFLINE / ONLINE
  → WAKING
  → PAIRING / READY
  → CONNECTING
  → STREAMING
  → RECONNECTING
  → DISCONNECTING
  → READY / OFFLINE / ERROR
```

UI state, diagnostics, and permitted actions must derive from the explicit session state rather than scattered booleans.

---

## 8. Platform Support

| Platform | Initial target | Important platform work |
| --- | --- | --- |
| Windows | Windows 11 x64; ARM64 retained if practical | D3D/DXVA, HDR, raw input, DPAPI, installer signing, handheld testing. |
| macOS | macOS 14+ on Apple Silicon; Intel evaluated separately | VideoToolbox, Metal, HDR/EDR, Keychain, notarization, controller permissions. |
| Linux | Current x86-64 distributions through Flatpak and AppImage | VAAPI/Vulkan, Wayland and X11, libsecret, portals, packaging variance. |
| Steam Deck/Bazzite | First-class Linux configuration | Gamescope, integrated controls, suspend/resume, docking, external displays, and HDR maturity. |

Platform parity is defined by user outcomes rather than identical implementation.

---

## 9. Security and Privacy

- No mandatory Lunaframe account or cloud dependency.
- Pairing credentials and wake-provider tokens stored in OS credential facilities.
- Host identity changes surfaced clearly.
- Themes are data-only and cannot execute code.
- External wake providers are explicitly configured and authenticated.
- Advanced peripheral transport is disabled by default and allowlisted if later implemented.
- Host actions respect server-side permissions.
- Destructive actions require confirmation.
- Logs redact credentials, tokens, private keys, clipboard contents, and sensitive identifiers.
- Telemetry is absent by default. Any future telemetry must be opt-in and documented.
- Tailscale provides connectivity, not implicit Lunaframe authorization.

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
- Contract tests using captured or simulated GameStream, Sunshine, Apollo, and Vibepollo responses.
- Controller mapping tests using virtual devices where practical.
- QML navigation tests proving that every primary action is controller-reachable.
- Snapshot tests for built-in themes and representative display sizes.
- Malformed-input tests for themes, host metadata, artwork, and capability responses.
- CI builds for Windows, macOS, and Linux on every merge.

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

## Milestone 0: Compatibility fork

**Objective:** Establish an independently branded client that streams successfully on all three desktop platforms.

Deliverables:

- Fork and rebrand Moonlight Qt as Lunaframe Client.
- Adopt separate application identifiers and configuration directories.
- Build Windows, macOS, and Linux packages.
- Pair with Vibepollo and stream one application.
- Validate H.264, HEVC, AV1, audio, keyboard, mouse, and a standard controller.
- Establish CI and baseline performance measurements.

Exit criteria:

- All three platforms can add a host, pair, launch, stream, and disconnect.
- Lunaframe can coexist with official Moonlight without overwriting its settings.

## Milestone 1: Modern shell and themes

Deliverables:

- New host, library, application, settings, and pairing screens.
- Deterministic controller focus.
- Controller glyph system.
- Lunaframe visual identity and design tokens.
- Built-in themes and safe theme-package schema.
- Favorites, recent applications, search, and host-state indicators.
- Direct local Wake-on-LAN.

Exit criteria:

- A new user can pair, select an application, stream, and disconnect without a keyboard or mouse.

## Milestone 2: Controller system

Deliverables:

- Controller manager and live visualization.
- Calibration, dead zones, remapping, and player ordering.
- Per-controller and per-game profiles.
- Reliable hot-plug behavior.
- Compatible and native-type transmission modes.
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

## Milestone 4: Host-aware enhancements

Deliverables:

- Vibepollo, Apollo, and Sunshine capability adapters.
- Display-mode and virtual-display visibility.
- Multi-monitor selection where supported.
- Clipboard and microphone surfaces that appear only when negotiated.
- Improved per-host and per-game profiles.
- External wake-provider client interface.

Exit criteria:

- Optional features activate correctly without breaking baseline Sunshine compatibility.

## Milestone 5: Adaptive quality and advanced input

Potential deliverables:

- Adaptive bitrate and quality controls.
- Better VRR and frame-pacing behavior.
- DualSense adaptive-trigger support where the protocol permits it.
- Carefully scoped specialty-controller support.
- Additional remote-desktop capabilities negotiated with future hosts.

---

## 13. Initial Engineering Backlog

1. Confirm Lunaframe name availability before public release.
2. Fork Moonlight Qt and establish an upstream-sync strategy.
3. Set `io.lunaframe.client` or the final application identifier.
4. Separate Lunaframe configuration from official Moonlight.
5. Produce reproducible Windows, macOS, and Linux development builds.
6. Add a Vibepollo compatibility smoke test.
7. Record baseline latency, frame pacing, and controller behavior.
8. Inventory dependencies and platform-specific technical debt.
9. Introduce host, controller, theme, wake-provider, credential, and display interfaces.
10. Implement a new QML home screen behind a feature flag.
11. Implement controller focus and glyph primitives.
12. Implement theme tokens and two proof themes.
13. Implement direct Wake-on-LAN and the waking-state machine.
14. Add redacted structured logging and diagnostic export.
15. Establish performance gates before deeper refactoring.

The first cycle intentionally excludes a host fork, Raspberry Pi service, Stream Deck plugin, account service, and VM broker.

---

## 14. Risks and Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Fork diverges from Moonlight | Security and compatibility updates become expensive | Keep `moonlight-common-c` close to upstream, minimize core edits, and sync regularly. |
| Vibepollo changes rapidly | Optional integration breaks | Isolate Vibepollo behavior and negotiate individual capabilities. |
| Cross-platform behavior differs | Unreliable releases | Define behavioral acceptance tests and maintain physical-device testing. |
| Controller changes regress latency or mappings | Core product failure | Preserve the current path initially, instrument it, and gate changes against tests. |
| Themes harm readability or performance | Poor UX | Use validated tokens, constrained assets, safe fallbacks, and overlay-specific limits. |
| Parsec-like ambitions trigger a premature host rewrite | Client never ships | Keep all initial milestones compatible with existing hosts and move host work to a separate proposal. |
| External wake integration becomes insecure | Remote-control exposure | Require explicit HTTPS configuration, scoped credentials, and no default public endpoint. |
| Large simultaneous refactors stall delivery | No usable release | Complete the compatibility fork and vertical slice before migrating dependencies. |
| GPL or trademark obligations are mishandled | Distribution problems | Retain notices, publish source, use distinct branding, and perform formal name review. |

---

## 15. Licensing and Upstream Relationship

Moonlight Qt is licensed under GPL-3.0. A distributed derivative should remain GPL-3.0-compatible, retain required notices, and provide corresponding source. Lunaframe must use a distinct name, application identifier, visual identity, and release channel so users do not mistake it for an official Moonlight release.

General bug fixes, platform fixes, tests, and isolated protocol improvements should be considered for upstream contribution. Lunaframe-specific UI, themes, profiles, and integrations may remain in the fork.

---

## 16. Success Criteria

Lunaframe Client succeeds when:

- Windows, macOS, and Linux users receive comparable core functionality.
- A controller-only user can perform every ordinary game-streaming workflow.
- Keyboard and mouse remote desktop feels deliberate rather than incidental.
- Local Wake-on-LAN works as an integrated wake-and-connect experience.
- An external Raspberry Pi wake service can be added through a narrow client adapter without becoming part of the client codebase.
- Controllers can be identified, tested, ordered, calibrated, remapped, and profiled.
- Themes provide meaningful personalization without executable extension code.
- Vibepollo users receive deeper integration while Sunshine compatibility remains intact.
- Transient network and device events no longer routinely terminate play.
- Diagnostics explain likely causes and remedies.
- Tailscale-connected hosts behave like first-class private hosts.
- Streaming performance is at least equivalent to the Moonlight baseline.

---

## 17. Recommended Immediate Next Step

Begin with a focused compatibility phase:

1. Confirm the project name and application identifiers.
2. Fork Moonlight Qt and establish the upstream remote.
3. Build and run the unmodified client on Windows, macOS, and Linux.
4. Confirm end-to-end streaming against the intended Vibepollo version.
5. Capture baseline performance and controller behavior.
6. Add architectural interfaces without changing runtime behavior.
7. Replace the home screen with a minimal controller-navigable QML prototype.
8. Demonstrate direct local Wake-on-LAN followed by automatic connection.
9. Validate a manually configured Tailscale host.

The phase should end with a distributable client prototype, not a host service or orchestration platform.

---

## 18. Initial Brand Assets

The initial visual exploration uses a luminous video frame, crescent-shaped orbital signal, and cyan-to-violet transport trail over a near-black and deep navy base.

| Asset | Working filename | Intended use |
| --- | --- | --- |
| Application icon concept | `lunaframe-app-icon-concept-v1.png` | Launcher icon, repository avatar, and early builds. |
| Wide hero concept | `lunaframe-steam-hero-concept-v1.png` | Steam/Steam Deck hero, repository banner, and landing page. |
| Portrait grid concept | `lunaframe-steam-grid-concept-v1.png` | Steam library portrait capsule. |

These are concept assets rather than production masters. The selected mark should be redrawn as deterministic SVG and exported into exact Windows, macOS, Linux, Steam, and accessibility variants.

---

## 19. Reference Projects

- Moonlight Qt: <https://github.com/moonlight-stream/moonlight-qt>
- Moonlight common C library: <https://github.com/moonlight-stream/moonlight-common-c>
- Vibepollo: <https://github.com/Nonary/Vibepollo>
- Apollo: <https://github.com/ClassicOldSong/Apollo>
- Sunshine: <https://github.com/LizardByte/Sunshine>
- Tailscale documentation: <https://tailscale.com/kb/>


