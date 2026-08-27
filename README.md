# Jochona Client

Jochona Client is a controller-first desktop game-streaming client for Windows, macOS, and Linux. It is a fork of [Moonlight PC](https://moonlight-stream.org), keeping its proven low-latency streaming core while replacing the surrounding application experience: a modern QML shell, a real controller manager, profile automation, integrated Wake-on-LAN, and diagnostics that explain themselves.

This repository is a history-preserving import of [moonlight-stream/moonlight-qt](https://github.com/moonlight-stream/moonlight-qt) (GPL-3.0), synced regularly from an `upstream` remote. Upstream commit history and copyright notices are retained in-tree; Jochona-specific changes are marked with `Jochona:` comments.

**Status:** Milestone 0 — compatibility fork. The client builds and streams as upstream today; the product plan lives in [`proposal.md`](proposal.md), binding terminology in [`CONTEXT.md`](CONTEXT.md), architecture decisions in [`docs/adr/`](docs/adr/), and repository/CI/release setup in [`docs/github-setup.md`](docs/github-setup.md).

## What Jochona adds (planned, per proposal)

- Controller manager: live visualization, calibration, remapping, per-game Controller Maps, Player Slot ordering
- Unified multi-host library with user-confirmed deduplication
- Streaming Profile automation across devices, displays, and games
- Extended Wake-on-LAN: waking state, manual overrides, external wake providers
- Capability-adaptive host integration (Vibepollo, Apollo, Sunshine)
- Modern theming (data-only theme packages) and a controller-first shell

## Upstream features inherited today

- Hardware accelerated video decoding on Windows, macOS, and Linux
- H.264, HEVC, and AV1 codec support (AV1 requires a capable host and GPU)
- YUV 4:4:4, HDR, and 7.1 surround support per host capability
- Gamepad support with force feedback and motion controls for up to 16 players
- Pointer capture and direct mouse control; system shortcut forwarding

## Building

Jochona builds with the upstream toolchain unchanged.

### Requirements

- **Windows:** Qt 6.11+ (MSVC), Visual Studio 2026, 7-Zip on `%PATH%` (installer builds)
- **macOS:** Qt 6.11+, Xcode 15+, `create-dmg` (DMG builds)
- **Linux:** Qt 5.12+ or 6.x, FFmpeg 4+, plus distro packages listed below
- **Steam Link:** [Steam Link SDK](https://github.com/ValveSoftware/steamlink-sdk) with `STEAMLINK_SDK_PATH` set; device limits apply (1080p60, 40 Mbps, no HDR)

Linux (Debian/Ubuntu) base packages:

```text
libegl1-mesa-dev libgl1-mesa-dev libopus-dev libsdl2-dev libsdl2-ttf-dev libssl-dev
libavcodec-dev libavformat-dev libswscale-dev libva-dev libvdpau-dev libxkbcommon-dev
wayland-protocols libdrm-dev qt6-base-dev qt6-declarative-dev libqt6svg6-dev qt6-wayland
qml6-module-qtquick-controls qml6-module-qtquick-templates qml6-module-qtquick-layouts
qml6-module-qtqml-workerscript qml6-module-qtquick-window qml6-module-qtquick
```

(RedHat/Fedora equivalents and Qt 5 package names: see the upstream [build docs](https://github.com/moonlight-stream/moonlight-docs/wiki) until Jochona docs exist.)

### Steps

1. `git submodule update --init --recursive`
2. Windows/macOS only: run `setup-deps.ps1` / `setup-deps.py`
3. Build: `qmake6 moonlight-qt.pro && make release` (macOS/Linux), or open in Qt Creator
4. Distribution builds: `scripts/generate-dmg.sh` (macOS), `scripts\build-arch.bat` + `scripts\generate-bundle.bat` (Windows, from a Qt prompt), `scripts/build-steamlink-app.sh` (Steam Link)

Embedded targets: `qmake6 "CONFIG+=embedded" moonlight-qt.pro`; slow GPUs: add `CONFIG+=gpuslow`.

## Relationship to upstream

- Sync: `upstream/master` merged into `main` at least weekly (see `docs/github-setup.md`)
- Fixes worth contributing upstream are contributed upstream first, then merged down
- Name, bundle id (`com.jochona.client`), settings namespace (`Jochona`), and release channel are deliberately distinct — Jochona coexists with an installed official Moonlight on the same machine

## License

GPL-3.0, same as upstream. Upstream copyright and third-party notices are retained; see `docs/research/dependency-license-inventory.md` for the full dependency/license audit.
