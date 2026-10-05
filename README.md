# Jochona Client

Jochona Client is a controller-first desktop game-streaming client for Windows, macOS, and Linux. It is a fork of [Moonlight PC](https://moonlight-stream.org), keeping its proven low-latency streaming core while replacing the surrounding application experience: a modern QML shell, a real controller manager, profile automation, integrated Wake-on-LAN, and diagnostics that explain themselves.

This repository is a history-preserving import of [moonlight-stream/moonlight-qt](https://github.com/moonlight-stream/moonlight-qt) (GPL-3.0), synced regularly from an `upstream` remote. Upstream commit history and copyright notices are retained in-tree; Jochona-specific changes are marked with `Jochona:` comments.

**Status:** Milestone 2 beta. The client now uses the Night Route interface and the SQLite settings model.

## What Jochona adds in Milestone 2

- A controller-first Night Route interface for desktop, handheld, and television layouts
- Unified Library Entries with Host scoring, favorites, hidden entries, recents, offline cache data, and user-confirmed merge/split grouping
- Settings Baseline, Streaming Profiles, sparse contextual patches, provenance, pins, and quality floors
- Controller Maps with calibration, remapping, persistent Player Slot Order, and streamed-input transforms
- Bounded Wake recovery, display contexts, audio-output recovery, Session settings, and reconnect controls
- Local History controls and previewed Support Bundle export with address redaction, Wake route and Beacon route state, and recent session outcomes
- Stable and preview update channels for Jochona releases

The separate Jochona Host project is under active development in its own repository: a Sunshine-derived fork that preserves baseline GameStream while exposing authenticated, versioned Jochona extensions (`/jochona/v1/...` — Host Volume control, Encoder Tuple preflight, and more) that this client already speaks. Its first hosting-hardware target is strict AV1 encoding on Windows with an NVIDIA RTX 5090.

## Upstream features inherited today

- Hardware accelerated video decoding on Windows, macOS, and Linux
- H.264, HEVC, and AV1 codec support (AV1 requires a capable host and GPU)
- YUV 4:4:4, HDR, and 7.1 surround support per host capability
- Gamepad support with force feedback and motion controls for up to 16 players
- Pointer capture and direct mouse control; system shortcut forwarding

## Install

No stable releases exist yet. Builds come from GitHub Actions artifacts —
see [`docs/install.md`](docs/install.md) for Windows/macOS/Linux (AppImage,
including Bazzite), fetching a build with `gh`, pairing with a Host, and
Wake-on-LAN notes.

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
4. Distribution builds:
   - macOS: `scripts/generate-dmg.sh`
   - Windows (from a Qt command prompt): `scripts\build-arch.bat Release` per architecture (run once from an x64 Qt prompt, once from an ARM64 Qt prompt — the script detects the architecture from the active Qt toolchain, not from an argument); once both are built, `scripts\generate-bundle.bat Release` combines them into the installer bundle
   - Linux AppImage: `scripts/build-appimage.sh` — needs `linuxdeploy-<arch>.AppImage` on `PATH` and the same from-source SDL3/FFmpeg/libva/libplacebo/dav1d stack CI builds (see `.github/workflows/build-appimage.yml`); most contributors should grab the CI artifact instead (`docs/install.md`)
   - Steam Link: `scripts/build-steamlink-app.sh`

Embedded targets: `qmake6 "CONFIG+=embedded" moonlight-qt.pro`; slow GPUs: add `CONFIG+=gpuslow`.

### Experimental PyroWave (Apple Silicon)

PyroWave is a GPU wavelet codec, disabled in default builds and never selected
by automatic codec negotiation. Build with
`qmake6 -r moonlight-qt.pro CONFIG+=pyrowave && make release`.
The Host must also be built with `-DSUNSHINE_ENABLE_PYROWAVE=ON`, have
`pyrowave_encoder = enabled`, and be restarted before connecting.

Select **PyroWave** under the Client's video codec setting, or pass
`--video-codec PyroWave` to `Jochona stream`. Both endpoints require a
compatible Apple Silicon Metal GPU. Only even-sized 8-bit SDR YUV 4:2:0 physical
capture is supported; HDR, 4:4:4, virtual displays and software decoding fail
explicitly. Unsupported endpoints do not silently fall back to H.264/HEVC/AV1.

This experimental decoder presents directly through Metal and does not use the
FFmpeg statistics/overlay compositor. See
[`libs/pyrowave-metal/NOTICE.md`](libs/pyrowave-metal/NOTICE.md) for upstream
provenance and maintenance limitations.


## Relationship to upstream

- Sync: `upstream/master` merged into `main` at least weekly (see `docs/github-setup.md`)
- Fixes worth contributing upstream are contributed upstream first, then merged down
- Name, bundle id (`com.jochona.client`), settings namespace (`Jochona`), and release channel are deliberately distinct — Jochona coexists with an installed official Moonlight on the same machine

## License

GPL-3.0, same as upstream. Upstream copyright and third-party notices are retained; see `docs/research/dependency-license-inventory.md` for the full dependency/license audit.
