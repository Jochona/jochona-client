# Moonlight / Sunshine / Apollo / Vibepollo — verified ecosystem facts

Fact-check reference for `proposal.md` (compiled 2026-08-27).
Method: upstream sources read from `master` via raw.githubusercontent.com, GitHub REST API for metadata, Qt/Tailscale docs where noted. Every claim carries a URL. Unconfirmed items are marked **UNVERIFIED**.

Verdicts: **TRUE** · **PARTLY** · **FALSE** (contradicted) · **NEW** (genuinely absent upstream).

Repo health snapshot (GitHub API):

| Repo | Stars | Forks | Open issues | License | Created | Last push | Latest release |
|---|---|---|---|---|---|---|---|
| [moonlight-qt](https://github.com/moonlight-stream/moonlight-qt) | 18,421 | 1,271 | 557 | GPL-3.0 | 2018-04-28 | 2026-08-26 | [v6.1.0](https://github.com/moonlight-stream/moonlight-qt/releases/tag/v6.1.0) |
| [Sunshine](https://github.com/LizardByte/Sunshine) | 40,595 | 2,064 | 144 | GPL-3.0 | — | active | [v2026.516.143833](https://github.com/LizardByte/Sunshine/releases/latest) (2026-05-16), topic `maintainer-wanted` |
| [Apollo](https://github.com/ClassicOldSong/Apollo) | 10,686 | 414 | 327 | GPL-3.0 | 2024-07-29 | **2026-05-21** | [v0.4.6](https://github.com/ClassicOldSong/Apollo/releases/latest) (published 2025-07-13) |
| [Vibepollo](https://github.com/Nonary/Vibepollo) | 934 | — | 93 | GPL-3.0 | 2025-08-22 | 2026-08-22 | [1.18.4-stable.3](https://github.com/Nonary/Vibepollo/releases/latest) (2026-08-19) |

---

## 1. moonlight-stream/moonlight-qt

### 1.1 Qt version — proposal §7.1 "Qt 6 and Qt Quick/QML": **TRUE**
* `app/app.pro` line 1: `QT += core quick network quickcontrols2 svg` → Qt Quick + QuickControls2 + SVG; **Qt Widgets is not linked at all**. UI is `app/gui/*.qml` (PcView, AppView, StreamView, SettingsView, ErrorDialog, `gui/pin`, `gui/settings`). <https://raw.githubusercontent.com/moonlight-stream/moonlight-qt/master/app/app.pro>
* README build requirements: **"Qt 6.11 SDK or later (earlier versions may work but are not officially supported)"** for Windows (MSVC only; MinGW unsupported; Visual Studio 2026) and macOS (Xcode 15+); **"Qt 6 is recommended, but Qt 5.12 or later is also supported"** for Linux, and the Steam Link SDK ships Qt 5.14. <https://raw.githubusercontent.com/moonlight-stream/moonlight-qt/master/README.md>
* Shipped v6.1.0 embedded **Qt 6.7.2** in Windows/macOS builds. <https://github.com/moonlight-stream/moonlight-qt/releases/tag/v6.1.0>
* Build system is **qmake** (`.pro`/`.pri`, `globaldefs.pri`, `CONFIG+=` feature flags, `qtquickcompiler` on Win/mac release) — matches proposal §7.1 "preserve upstream initially". Video renderers: FFmpeg + libplacebo (VAAPI/VDPAU/Vulkan/WGC…), audio via SDL; none of that is QML.

Implication: "Qt 6 + QML" is the *incumbent* architecture, so it cannot be a differentiator; and choosing Qt-6.11-only silently drops the Qt 5 Linux build path upstream still supports.

### 1.2 Wake-on-LAN — already shipped end-to-end (**proposal §6.5 / M1 / §14 present it as new**)
* Magic packet: `NvComputer::wake()` builds `FF×6 + MAC×16` (102 bytes) and sends to every address the client knows for the host on **static ports 9 and 47009** (47009 = "Port opened by Moonlight Internet Hosting Tool for WoL") and **dynamic GFE ports 47998, 47999, 48000, 48002, 48010** re-based on the host's HTTP base port; refuses with "has no MAC address stored" when empty. <https://raw.githubusercontent.com/moonlight-stream/moonlight-qt/master/app/backend/nvcomputer.cpp>
* MAC origin: GameStream `/serverinfo` element **`<mac>`** (`getXmlString(serverInfo, "mac")`), with the `00:00:00:00:00:00` placeholder explicitly ignored, persisted as bytes under QSettings key `mac`. <same file>
* UX today: `ComputerModel` exposes role `wakeable` = `!computer->macAddress.isEmpty()` and action `wakeComputer(index)` → `DeferredWakeHostTask` → `NvComputer::wake()`; details string prints `MAC Address: %1`. <https://raw.githubusercontent.com/moonlight-stream/moonlight-qt/master/app/gui/computermodel.cpp>
* Sunshine publishes `mac` **only for paired requests over HTTPS**, derived from the local interface the client reached; over plain HTTP it sends `00:00:00:00:00:00` ("placeholder MAC address that Moonlight knows to ignore"). <https://raw.githubusercontent.com/LizardByte/Sunshine/master/src/nvhttp.cpp>
* Tailscale: no native WoL forwarding (L2 magic packets are not routable); the documented pattern is an always-on tailnet node on the target LAN. <https://tailscale.com/blog/wake-on-lan-tailscale-upsnap> → proposal §6.5 "Remote Wake-on-LAN generally requires an always-on device on the destination LAN" is **TRUE**.

Real remaining gaps (worth keeping as features): a *waking* state machine, subnet/broadcast-candidate selection, per-host WoL port/relay overrides, and external wake-provider adapters.

### 1.3 Updates per platform (**§6.15 wants signed installers + channels**)
* `AutoUpdateChecker::start()` is compiled only `#if defined(Q_OS_WIN32) || defined(Q_OS_DARWIN) || defined(STEAM_LINK) || defined(APP_IMAGE)` — comment: "Only run update checker on platforms without auto-update". <https://raw.githubusercontent.com/moonlight-stream/moonlight-qt/master/app/backend/autoupdatechecker.cpp>
* Manifest: `GET https://moonlight-stream.org/updates/qt.json`; each entry needs `platform`, `arch`, `version`, `browser_url` (+ optional `kernel_version_at_least`); matched against `QSysInfo::buildCpuArchitecture()` and a platform string (`windows`, `osx`, `steamlink`, `appimage`); emits `onUpdateAvailable(version, browser_url)`. <https://raw.githubusercontent.com/moonlight-stream/moonlight-qt/master/app/backend/autoupdatechecker.cpp>
* `app/gui/main.qml`: an update button appears and `Qt.openUrlExternally(browserUrl)` opens the download page (only when `SystemProperties.hasBrowser`). **No in-app download/install.** <https://raw.githubusercontent.com/moonlight-stream/moonlight-qt/master/app/gui/main.qml>
* Linux `.deb`/distro builds: no self-update. Snap/Flatpak builds rely on the store — **UNVERIFIED** in this pass.

### 1.4 Gamepad path (**§6.2, §7.1, §7.4**)
* Transport: SDL2 (`app.pro` links SDL2; SDL2_ttf for overlays), one `SdlInputHandler` with up to `MAX_GAMEPADS` slots, events batched from the SDL queue. <https://raw.githubusercontent.com/moonlight-stream/moonlight-qt/master/app/streaming/input/gamepad.cpp>
* Identity sent to host: `SDL_GameControllerGetType()` → `LI_CTYPE_XBOX` (Xbox360/One), `LI_CTYPE_PS` (**PS3 *and* PS4 *and* PS5**), `LI_CTYPE_NINTENDO` (Switch Pro / 2), `LI_CTYPE_STEAM` (VID/PID table), else `LI_CTYPE_UNKNOWN`; `LiSendControllerArrivalEvent(index, mask, type, buttonFlags, capabilities)`. → proposal §6.2 mode 2 "preserve … DualSense, DualShock … identity" is **not expressible on the wire today**: there is no DualSense-vs-DualShock type. **PARTLY FALSE as a client-only feature.**
* Already supported per controller: `LI_CCAP_ANALOG_TRIGGERS`, `LI_CCAP_RUMBLE`, `LI_CCAP_TRIGGER_RUMBLE`, `LI_CCAP_TOUCHPAD`, `LI_CCAP_DUAL_TOUCHPAD`, `LI_CCAP_ACCEL`, `LI_CCAP_GYRO`, `LI_CCAP_BATTERY_STATE`, `LI_CCAP_RGB_LED`. <https://raw.githubusercontent.com/moonlight-stream/moonlight-common-c/master/src/Limelight.h>
* **Motion sensors already work**: host requests a rate through `ConnListenerSetMotionEventState(controllerNumber, motionType, reportRateHz)`; the client sets `accelReportPeriodMs`/`gyroReportPeriodMs`, calls `SDL_GameControllerSetSensorEnabled()`, and streams `LiSendControllerMotionEvent(..., LI_MOTION_TYPE_ACCEL|GYRO, x,y,z)`. Rumble triggers, LED, battery, and **DualSense adaptive triggers** (`ConnListenerSetAdaptiveTriggers`; PS5 output reports replayed via `SDL_GameControllerSendEffect`) are likewise implemented. → proposal §6.2 "live visualization of … motion sensors", §7.4 `"controller_motion": true`, and §M5 "DualSense adaptive-trigger support" **already exist upstream**; only host-side support varies. **FALSE as novelty.**
* Mappings are data-driven: community SDL gamecontrollerdb downloaded from `https://moonlight-stream.org/SDL_GameControllerDB/gamecontrollerdb.txt`. <https://raw.githubusercontent.com/moonlight-stream/moonlight-qt/master/app/settings/mappingfetcher.cpp>
* Host-side emulation is not the client's business: Sunshine requires **ViGEmBus on Windows** and **uinput on Linux** for virtual gamepads, and fetches a per-app mapping string (`?gcmapp=`, `getGcmMap()` in <https://raw.githubusercontent.com/LizardByte/Sunshine/master/src/process.cpp>). No ViGEmBus code exists in the client. **UNVERIFIED:** Sunshine macOS pad driver specifics.

### 1.5 Settings storage (**§7.1 "versioned SQLite", §6.4 "OS credential stores", §M0 "don't overwrite Moonlight settings"**)
* Identity: `setOrganizationName("Moonlight Game Streaming Project")`, `setOrganizationDomain("moonlight-stream.com")`, `setApplicationName("Moonlight")`; `QSettings::setDefaultFormat(IniFormat)` + `setPath(..., QDir::currentPath())` are applied **only when `./portable.dat` exists**. <https://raw.githubusercontent.com/moonlight-stream/moonlight-qt/master/app/main.cpp>
* Therefore non-portable builds use **native format**: Windows registry `HKEY_CURRENT_USER\Software\Moonlight Game Streaming Project\Moonlight`; macOS `~/Library/Preferences/com.moonlight-stream.Moonlight.plist`; Linux `~/.config/Moonlight Game Streaming Project/Moonlight.conf`. <https://doc.qt.io/qt-6/qsettings.html>
* Contents (`NvComputer::serialize/deserialize`): `uuid`, `mac`, `localaddress/localport`, `remoteaddress`, server cert PEM, `IsNvidiaServerSoftware`, applist array (per-app `MaxFps`, `MaxResolution`, HDR flag, bitrate override) — a concrete, greppable key namespace to migrate. <https://raw.githubusercontent.com/moonlight-stream/moonlight-qt/master/app/backend/nvcomputer.cpp>
* **Pairing identity (self-signed X509 + private key, OpenSSL) is stored inside that same settings store** (`IdentityManager::createCredentials(QSettings&)`) — no OS keychain today. <https://raw.githubusercontent.com/moonlight-stream/moonlight-qt/master/app/backend/identitymanager.cpp>
* So: SQLite/schema-versioned profile store and OS credential stores are **NEW** work; changing org/app name is sufficient for §M0 coexistence.

### 1.6 Discovery, reachability, and what already exists (§4.4, §6.4, §6.6)
* LAN discovery = mDNS browse of **`_nvstream._tcp.local.`** via QMdnsEngine, behind an `enableMdns` preference; manual add path `addNewHost(address, mdns=false,…)`; WAN address filled by STUN (`stun.moonlight-stream.org:3478`); blocked-port diagnosis via `LiTestClientConnectivity`. <https://raw.githubusercontent.com/moonlight-stream/moonlight-qt/master/app/backend/computermanager.cpp>
* Reachability classification already exists: `getActiveAddressReachability()` returns `RI_UNKNOWN / RI_LAN / RI_VPN` using interface heuristics — PPP/virtual interfaces, `MTU < 1500`, tun/tap name check, MAC prefix `00:FF`, name prefix `ZeroTier`, name containing `VPN`, then an on-link/prefix test. **No Tailscale-specific code exists**; Tailscale is classified `RI_VPN` through the MTU heuristic. **PARTLY FALSE as novelty / [INFERENCE] for the Tailscale-MTU claim.** <https://raw.githubusercontent.com/moonlight-stream/moonlight-qt/master/app/backend/nvcomputer.cpp>
* Host family is inferred: `isNvidiaServerSoftware = state.contains("MJOLNIR")` (GFE), with Sunshine reporting `SUNSHINE_SERVER_BUSY|FREE`.
* Genuinely absent upstream (**NEW**): cross-host unified library (single `sessionHost`; app lists strictly per-`NvComputer`), favourites/collections, per-game profile store, theming system, in-session controller/profile/bitrate overlay management, reconnection across network changes, Tailscale-aware host labelling.
* Already upstream per v6.1.0 notes / README: multisession, HDR10 + Dolby Vision + ColorBand metadata, 10-bit, AV1, VRR, surround up to 7.1, hardware decoding Win/mac/Linux, libplacebo tone-mapping, PiP, up to 4K120, "up to 16 players".

### 1.7 License
`LICENSE` at repo root = plain **GPL-3.0** (GitHub SPDX `gpl-3.0`; same blob as moonlight-common-c's). Vendored pieces carry their own notices (SDL fork, qmdnsengine, OpenSSL/FFmpeg/libplacebo linkage) — relevant to proposal §13.

---

## 2. moonlight-stream/moonlight-common-c

Header: <https://raw.githubusercontent.com/moonlight-stream/moonlight-common-c/master/src/Limelight.h>; negotiation: `src/SdpGenerator.c`, `src/RtspConnection.c`.

### 2.1 Host info available to a client
`SERVER_INFORMATION` carries the raw `/serverinfo` payload plus `serverCodecModeSupport`. The **MAC address is not a library field** — clients parse XML `<mac>` themselves, which is why the client, not the library, owns WoL.

### 2.2 Codecs, HDR, colour — proposal §M0/§7.4 `hdr`/`av1`: **TRUE**
* H.264 / HEVC / AV1 negotiated by `ServerCodecModeSupport` bitmask (Sunshine computes it, incl. `SCM_AV1_HIGH10_444`) + `MaxLumaPixelsHEVC` gate. <https://raw.githubusercontent.com/LizardByte/Sunshine/master/src/nvhttp.cpp>
* HDR metadata (EOTF, MaxCLL/MaxFALL, mastering display) part of stream config; HDR gated **per application** by `IsHDRSupported` in `/applist`.
* Chroma/4:4:4 and 10-bit paths exist upstream.

### 2.3 Gamepad / input capability surface
Per-controller capability bits at arrival: `LI_CCAP_ANALOG_TRIGGERS 0x01`, `LI_CCAP_RUMBLE 0x02`, `LI_CCAP_TRIGGER_RUMBLE 0x04`, `LI_CCAP_TOUCHPAD 0x08`, `LI_CCAP_ACCEL 0x10`, `LI_CCAP_GYRO 0x20`, `LI_CCAP_BATTERY_STATE 0x40`, `LI_CCAP_RGB_LED 0x80`, `LI_CCAP_DUAL_TOUCHPAD 0x100`; APIs `LiSendControllerArrivalEvent`, `LiSendControllerTouchEvent2`, `LiSendControllerMotionEvent`, `LiSendControllerBatteryEvent`; host→client callbacks `ConnListenerSetMotionEventState`, `ConnListenerSetControllerLED`, `ConnListenerSetAdaptiveTriggers`, `ConnListenerRumbleTriggers`. Controller *type* vocabulary limited to `LI_CTYPE_XBOX/PS/NINTENDO/STEAM(/UNKNOWN)` — the ceiling behind "native DualSense identity".

### 2.4 In-band capability negotiation beyond version strings — exists, and is small
* Client→host SDP `x-ml-general.featureFlags` (`ML_FF_FEC_STATUS 0x01 | ML_FF_SESSION_ID_V1 0x02`); host→client `x-ss-general.featureFlags` (`SunshineFeatureFlags`); encryption caps `SS_ENC_CONTROL_V2/SS_ENC_VIDEO/SS_ENC_AUDIO`; legacy GFE `x-nv-general.featureFlags` gated by `APP_VERSION_AT_LEAST`.
* Version comparison dominates the ecosystem — proposal §4.4's "must not assume behavior from a version number" cannot apply to baseline Sunshine, where version *is* the protocol. **Nothing today is a declarative capability registry** except Vibepollo's (§5).

---

## 3. LizardByte/Sunshine

Client-visible GameStream surface on master (`src/nvhttp.cpp`) is **smaller than the proposal assumes**:
* HTTPS: `/serverinfo`, `/pair`, `/applist`, `/appasset`, `/launch`, `/resume`, `/cancel`; HTTP: `/serverinfo`, `/pair`.
* **No `/serverconfig`, `/state`, `/serverdisplaymodes`, `/serverresolution`, `/serveraudio`, `/displaydevice`, `/actions/*`, `/bitrate`, clipboard endpoint, or quota endpoint.** GFE's `ApplicationQuota` is never read by moonlight-qt.

`/serverinfo` fields verbatim: `hostname`, `appversion`, `GfeVersion` (fixed constant), `uniqueid`, `HttpsPort`, `ExternalPort`, `MaxLumaPixelsHEVC`, `mac` (HTTPS/paired only; `00:00:00:00:00:00` over HTTP), `LocalIP`, `ServerCodecModeSupport`, optional `ExternalIP`, `PairStatus`, `currentgame`, `state` = `SUNSHINE_SERVER_BUSY|FREE`.

Clipboard: no endpoint on master; community consensus one-way client→host keystroke trick only. <https://github.com/LizardByte/Sunshine/issues/1539> → proposal §6.9/§M4 clipboard-on-Sunshine is **not satisfiable**.

---

## 4. ClassicOldSong/Apollo

**What it is:** GitHub reports `fork: false` (detached lineage) but self-described *"Sunshine fork - the easiest way to stream with the native resolution of your client device"*; GPL-3.0; 10,686 stars; last push 2026-05-21. <https://api.github.com/repos/ClassicOldSong/Apollo>

**Client-detected capability surface** (`src/nvhttp.cpp`):
* `/serverinfo` extras for paired clients: `scaleFactor`, **`VirtualDisplayCapable`**, **`VirtualDisplayDriverReady`**, **`Permission`** bitmask (`allow_launch`, `allow_exit`, `allow_keyboard`, `allow_clipboard`) — per-client authorization absent upstream.
* Rich running state (`currentgameuid/pid/name/id`, `running`, `appuuid`) on `/serverinfo` and `/state`.
* Extra endpoints: `/actions/volumes`, `/actions/cancel`, `/actions/toggle`, **`/actions/clipboard`** (GET/POST, gated by `allow_clipboard`), `/clientaudio`, `/serverdisplaymodes`, `/serverresolution`, `/serveraudio`, `/displaydevice`, `/action/bitrates`.
* Launch knobs: `virtualDisplay` policy (`always`/`apprequest`/`force`), `scaleFactor`, `hdrMode`, `clipLaunch`, `clientName`, `encAudio`/`surroundAudio`, `gamepads`.

**Detection:** no Apollo identifier field — presence-probing of extra keys/endpoints is the only mechanism. §7.4 per-capability probing is therefore the correct design; §4.4's anti-version stance is unachievable for baseline Sunshine.

---

## 5. Nonary/Vibepollo

**What it is:** `fork: true`, parent **ClassicOldSong/Apollo**; GPL-3.0; created 2025-08-22; last push 2026-08-22; 934 stars. <https://api.github.com/repos/Nonary/Vibepollo> → lineage **Sunshine → Apollo → Vibepollo**; the "Vibepollo primary, Apollo/Sunshine compatibility" ladder is coherent (superset ⊃ subset), but Vibepollo is ~1 year old, ~3 months behind Apollo master, and Apollo itself is semi-dormant.

**Platform support:** latest release ships one asset, `Vibepollo-win-x64-1.18.4-stable.3.exe` → **Windows x64 only** in practice. **UNVERIFIED:** non-Windows CI.

**Client-visible extended API** (`src/nvhttp.cpp`):
* Routes: GameStream set + `/unpair`, `/appset`, `/actions/clipboard`, **`/bitrate`**, **`/api/abr/capabilities`**.
* **The one true capability endpoint in the ecosystem:** `GET /api/abr/capabilities` → `{"supported":false,"version":1,"features":["runtime_bitrate"]}` (permission-gated); server-side ABR intentionally unimplemented — "Foundation-compatible clients … drive their own local ABR controller … through the runtime `/bitrate` endpoint". `GET /bitrate?bitrate=N` applies live and returns the applied value.
* `/serverinfo` publishes `VirtualDisplayCapable`/`VirtualDisplayDriverReady` **even unpaired**, `Permission`, `currentgameuid`, `appuuid`, `ServerCodecModeSupport`.
* Launch params: `virtualDisplay`, `scaleFactor`, `hdrMode`, `enable_hdr`, `clientName`, **`clientVrrRequested`**, `bitrate`, `framenogen`, `output_name_override`, `app_metadata`, `gamepad`, `surroundAudio/Params`, `enable_sops`.

**Caveats:** "display mode matching" is a *request* (launch params + host virtual-display driver), not a handshake; no endpoint lists modes. Vibepollo's Web UI API (`src/api.cpp`, username/password auth) is separate from the client-cert model — **UNVERIFIED** route list. `microphone_input` (§7.4 example) — **UNVERIFIED** protocol-level mic support on all three hosts.

---

## 6. "Lunaframe" name collision check

* GitHub repo search → 6 repos (`Atlinnner/LunaFramework` Java, `TennesseeLunabotics/lunaframe` C++, `tfurur/LunaFrame` TS, etc.), negligible traffic.
* **No GitHub organization** `lunaframe` — but the **handle is taken by a user account** `lunaframe` (id 292085501, created 2026-06-09, 2 public repos). GitHub forbids an org with the same name as any user login → **an org literally named `lunaframe` is impossible** while that user exists. <https://api.github.com/users/lunaframe>
* **UNVERIFIED:** npm/PyPI/crates.io/domain/app-store/commercial collisions, trademark registration.

→ Proposal backlog item 1 ("Confirm Lunaframe name availability") is well-founded and now concrete.

---

## 7. Verdict table for the proposal's load-bearing claims

| Proposal claim | Verdict | Basis |
|---|---|---|
| §7.1 Qt 6 + Qt Quick/QML "existing Moonlight architecture" | **TRUE** | app.pro; README Qt 6.11 (Win/mac), Qt 5.12+ on Linux |
| Preserve Moonlight's low-latency core | **TRUE** | §1, §2 |
| §6.5/M1 direct local Wake-on-LAN as new client work | **PARTLY FALSE** | Already shipped: `<mac>` → packet, ports 9/47009 + GFE set, `wakeComputer()` |
| MAC obtainable from host | **TRUE, caveat** | HTTPS/paired only on Sunshine; HTTP = placeholder → manual MAC entry needed |
| Remote WoL needs an always-on LAN relay | **TRUE** | Tailscale blog |
| §4.4 capability discovery missing today | **PARTLY TRUE** | Codec flags + SDP flags + launch params only; Apollo fields; Vibepollo `/api/abr/capabilities` |
| §4.4 "never assume from version number" | **PARTLY FALSE (unachievable for baseline)** | `APP_VERSION_AT_LEAST` *is* the protocol on GFE/Sunshine |
| §6.2 mode 2 "preserve DualSense vs DualShock identity" | **PARTLY FALSE** | `LI_CTYPE_PS` collapses PS3/PS4/PS5 |
| §6.2 motion visualization, §7.4 `controller_motion`, M5 adaptive triggers | **FALSE as novelty** | All implemented upstream; host support varies |
| §6.9 clipboard "when supported by the host" | **TRUE on Apollo/Vibepollo only** | Absent on Sunshine |
| §6.8/M4 virtual display / mode matching / HDR / VRR via Vibepollo | **TRUE as host-side request knobs**, not a handshake | §5 |
| §7.1 SQLite store; §6.4 OS credential stores | **NEW (real work)** | QSettings native store; key+cert inside it |
| M0 coexistence with Moonlight settings | **EASY** (rename org/app identity) | §1.5 |
| §6.4 LAN discovery + pairing | **TRUE (exists)** | mDNS `_nvstream._tcp.local.` |
| §6.6 LAN vs overlay distinction | **PARTLY FALSE (VPN heuristics exist)** | `getActiveAddressReachability()` |
| §6.7 unified library / profiles / themes / overlay / resilience | **NEW** | single `sessionHost`; per-`NvComputer` lists |
| §7.1 preserve qmake build initially | **TRUE** | app.pro |
| §13/§15 GPL-3.0 from moonlight-qt only | **TRUE** | license files; hosts' GPL irrelevant to a client |
| "Lunaframe" available | **NOT ESTABLISHED** | GitHub user owns handle; org name impossible; UNVERIFIED elsewhere |

## 8. Outstanding UNVERIFIED items
1. Which Sunshine module advertises `_nvstream._tcp` over mDNS (client side verified only).
2. Sunshine's macOS virtual-gamepad mechanism; exact `/launch` arg completeness.
3. moonlight-qt Snap/Flatpak store-based self-update manifests.
4. On-disk config paths observed by running the app (derived from identity + Qt docs, not executed).
5. Qt 6.11 LTS designation.
6. Vibepollo `src/api.cpp` route inventory; non-Windows CI.
7. Protocol-level microphone support on any host.
8. Non-GitHub "Lunaframe" collisions (npm/PyPI/domains/app stores/trademarks).
