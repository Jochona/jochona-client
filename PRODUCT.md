# Product

<!-- impeccable:product-schema 1 -->

## Platform

desktop

Qt Quick Controls 2 (Qt 6) native desktop application: macOS 14+ Apple Silicon,
Windows 11 x64, current Linux x86-64 (AppImage/Flatpak), Steam Link. Impeccable
web live mode and ios/android platform references do not apply.

## Users

- Private gaming-cloud owner: streams from their own always-on PC (Vibepollo /
  Apollo / Sunshine hosts) to their own devices; wants it to just work and stay
  local.
- Trusted friend: connects to someone else's host over Tailscale; no shared
  Jochona account exists or may be required.
- Controller-first living-room player: TV + one gamepad; every ordinary
  workflow must be completable without keyboard or mouse.
- Handheld player (Steam Deck / ROG Ally): docking/undocking, display changes,
  battery; profiles follow the device/display.
- Laptop/remote-desktop user: keyboard + mouse forwarding, pointer capture,
  Parsec-like desktop usability.

## Product Purpose

Jochona Client is a rebranded Moonlight Qt fork that becomes the nicest way to
game-stream to a self-hosted PC. Success (§16): controller-only users complete
every ordinary workflow; Wake-on-LAN works end-to-end including wrong cached
MACs; controllers can be identified, tested, ordered, calibrated, remapped;
diagnostics explain likely causes; performance is at least upstream parity.

## Positioning

Capability-driven local-first streaming: a normalized internal capability model
(Vibepollo's declarative endpoint, Apollo's probe fields, Sunshine's baseline)
drives every optional UI — no accounts, no cloud, no version-string guessing
outside the Sunshine adapter. Host features appear only when the host actually
has them.

## Operating Context

LAN and private-overlay (Tailscale) networks; hosts running Sunshine, Apollo,
or Vibepollo; Wake-on-LAN and optional external wake service (user's Raspberry
Pi); gamepads (Xbox, DualSense, Switch Pro, Steam Deck) wired and Bluetooth;
multi-display, HDR, VRR. v1 is English only; all strings pass Qt Linguist
extraction enforced in CI.

## Capabilities and Constraints

- Incremental QML screen replacement behind feature flags (ADR-0001); upstream
  screens remain reachable as escape hatch.
- One SQLite database (schema-migration runner, `VACUUM INTO` backups) for
  settings, profiles, library cache, history — frozen decision; secrets live
  only in OS credential stores (Keychain/DPAPI/libsecret), never SQLite.
- Host identity change hard-blocks streaming until deliberate re-pairing; no
  soft-continue in v1.
- Controller Maps run client-side before protocol send (work against every
  host); wire identity caps at family vocabulary (Xbox/PS/Nintendo/Steam).
- Overlay renders independently of the streamed frame; exclusive fullscreen
  deliberately excluded (ADR-0003).
- Themes are versioned, data-only packages; invalid themes fail to built-in.
- Updates are check-and-notify only; no in-app install in v1.
- Adaptive streaming ships as guided connection test first; client-driven ABR
  is post-1.0 against Vibepollo's /bitrate endpoint.
- GPL-3.0; full upstream history preserved (ADR-0002).

## Brand Commitments

Name: Jochona (`com.jochona.client`, jochona.com). Identity: neon-on-navy —
crescent + play-mark + orbital trail; Space Grotesk display, Inter UI, Noto
Emoji (bundled OFL); rimless glossy app icon. Voice (UI copy): plain, direct,
names the action; errors name the problem and the recovery.

## Evidence on Hand

- `proposal.md` — full spec (this record summarizes; spec wins on detail).
- `docs/research/moonlight-ecosystem-facts.md` — verified host API facts.
- `docs/adr/` — ADR-0001 incremental shell, ADR-0002 history import,
  ADR-0003 no exclusive fullscreen.
- `assets/brand/` — tracked masters; `scripts/generate-brand-assets.py`
  regenerates every platform asset.
- `assets/vendor/kenney-input-prompts-1.5.zip` — CC0 controller glyphs.
- `app/gui/HomeView.qml` + `app/gui/style/Tokens.qml` — shipped modern home
  screen and design tokens behind `modernHomeScreen`.
- Absences: no telemetry, no benchmarks, no testimonials — never fabricate.

## Product Principles

1. Excellent defaults, visible control.
2. Controller-first, not controller-only.
3. Direct by default; capability-driven, never product-name-driven.
4. Recover rather than fail.
5. Local ownership: no account, no cloud dependency, secrets in the OS vault.

## Accessibility & Inclusion

Complete keyboard and controller navigation; deterministic D-pad focus with no
unreachable or ambiguous targets; high-contrast theme; adjustable interface
scale; reduced-motion mode; status never color-only; remappable app shortcuts;
configurable hold durations and repeat behavior.
