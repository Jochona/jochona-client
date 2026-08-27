# Dependency & License Inventory — Jochona Client

Date: 2026-08-27. Scope: everything the build/packaging pipeline compiles in or
ships in a distributable artifact. Evidence rule: every license claim cites a
path in this repo. **Where the repo does not prove a license, the entry is
marked `UNKNOWN`.** No license in this document was filled in from memory;
"publicly reported" notes are verification pointers, not findings.

This file is the audit that `README.md` points at ("see
`docs/research/dependency-license-inventory.md` for the full
dependency/license audit").

## 1. The Jochona work itself

| Item | License | Evidence |
|---|---|---|
| App code (`app/`, `AntiHooking/`, `masterhook*.c`, `h264bitstream/` vendored dir, scripts) | GPL-3.0-or-later (declared) | Full GPL-3.0 text at `LICENSE:1-3`; `README.md` "GPL-3.0, same as upstream"; `app/deploy/linux/com.jochona.client.appdata.xml:5` `<project_license>GPL-3.0+</project_license>` |

No source file in `app/` or `AntiHooking/` carries an individual license header
(grep for GPL / "at your option" notices in `app/main.cpp`,
`AntiHooking/antihookingprotection.cpp` found none); the whole-tree LICENSE file
plus README/appdata declarations are the only license statements for the app.

Submodules are pinned in `.gitmodules`: `moonlight-common-c` (`.gitmodules:1-2`),
`qmdnsengine` (`.gitmodules:3-4`), `SDL_GameControllerDB` (`.gitmodules:5-6`).
`h264bitstream/` and `AntiHooking/` are **vendored** sources tracked directly in
this repo, not submodules.

## 2. What each shipped artifact contains

| Artifact | Contents (proven by) |
|---|---|
| Windows `MoonlightSetup-<ver>.exe` bundle (built `scripts/generate-bundle.bat:77,81`) | Bootstrapper (displays GPL text via `wix/MoonlightSetup/Bundle.wxs:54` → `wix/MoonlightSetup/license.rtf:4-5`), VC++ Redist **downloaded from Microsoft at install time**, not embedded (`Bundle.wxs:2-13,63-101`), and two per-arch MSIs (`Bundle.wxs:103-116`). |
| Windows `Moonlight.msi` | `Moonlight.exe` plus harvest of the whole deploy dir (`wix/Moonlight/Product.wxs:102-104`). Deploy dir = prebuilt-lib DLLs (`scripts/build-arch.bat:230`), `AntiHooking.dll` (`:233-235`), `gamecontrollerdb.txt` (`:238`), Qt runtime + QML modules via windeployqt (`:256`; `icuuc.dll` deleted at `:272`). |
| Windows portable `MoonlightPortable-<arch>-<ver>.zip` (`scripts/build-arch.bat:318`) | Same deploy dir **plus VC CRT DLLs copied out of Visual Studio** (`scripts/build-arch.bat:145,300-303`). |
| macOS `Jochona-<ver>.dmg` (`scripts/generate-dmg.sh:81-83,99`) | `Jochona.app` after macdeployqt (`:69`) — embeds Qt frameworks plus everything from `libs/mac` (`app/app.pro:588-593`). |
| Linux AppImage (`scripts/build-appimage.sh:67-69`) | App plus linuxdeploy-qt-plugin bundle and `libSDL3.so.0` (`:68`); builds against apt Qt6 (`build-appimage.yml:32`). CI builds SDL3, sdl2-compat, SDL_ttf, libva, patched libplacebo, dav1d, FFmpeg from source into the image (`build-appimage.yml:42-147`). |
| Steam Link `Moonlight-SteamLink-<ver>.zip` (`scripts/build-steamlink-app.sh:56`) | **Only** our `moonlight` binary (`:53`) plus `app/deploy/steamlink/` metadata (`:54`). No SDK files are copied. CI uploads exactly that (`build-steamlink.yml:37-42`). |
| Source tarball `MoonlightSrc-<ver>.tar.gz` (`scripts/generate-src.sh:18`) | Superproject **and all submodules** via `scripts/git-archive-all.sh` (`:6-8,250-262`). |

## 3. Direct build/runtime dependencies

### 3.1 In-tree and submodule sources (statically linked into every binary)

| Dep | Link mode / evidence | License (proof) | GPL-3.0 verdict |
|---|---|---|---|
| moonlight-common-c (submodule) | static lib (`moonlight-common-c/moonlight-common-c.pro:12-13`), linked `app/app.pro:505-507` | GPL-3.0 — verbatim text at `moonlight-common-c/moonlight-common-c/LICENSE.txt:1-2`; no per-file "or later" notice found → treat as GPL-3.0-only | **Compatible** (GPL + GPL); app must be conveyed under GPL terms — already the case. |
| enet (inside the moonlight-common-c checkout) | compiled into the static lib (`moonlight-common-c.pro:44-52`) | MIT — `moonlight-common-c/moonlight-common-c/enet/LICENSE:1-3` (Lee Salzman 2002-2020) | **Compatible.** MIT notice retention required in distributions. |
| nanors (inside the moonlight-common-c checkout) | compiled in (`moonlight-common-c.pro:53-55`) | MIT — `moonlight-common-c/moonlight-common-c/nanors/LICENSE:1-3` (Joseph Calderon 2021) | **Compatible.** Notice retention required. |
| qmdnsengine (submodule) | static lib (`qmdnsengine/qmdnsengine.pro:7-8`), linked `app/app.pro:512-514` | MIT — `qmdnsengine/qmdnsengine/LICENSE.txt:3,5` (Nathan Osman 2018) | **Compatible.** Notice retention required. |
| h264bitstream (vendored) | static lib (`h264bitstream/h264bitstream.pro:13-14`), linked `app/app.pro:519-521` | **LGPL-2.1 text** shipped at `h264bitstream/LICENSE:1-2`; no copyright holder named and no per-file notice anywhere in the directory (grep found none) → only/or-later indeterminate; upstream provenance and true license **UNKNOWN** from repo contents. | **CONDITIONAL / VERIFY.** LGPL-2.1-only + static linking into a GPL-3.0+ binary is the classic license mismatch; safe only if "or later" is selectable (→ LGPL-3.0) or upstream is actually permissive. Obtain the upstream license statement. |
| SDL_GameControllerDB (submodule; data file) | `gamecontrollerdb.txt` embedded in the exe (`app/qml.qrc:83`) and copied beside the exe on Windows (`scripts/build-arch.bat:238`) | zlib-style SDL license — `app/SDL_GameControllerDB/LICENSE:1-9` (Sam Lantinga 1997-2025) | **Compatible.** Its copyright/permission notice is *not* currently shipped in any artifact — retention gap (action 2). |
| AntiHooking | vendored in-tree, linked `app/app.pro:527-528`; shipped as Windows DLL (`scripts/build-arch.bat:233-235`) | Part of the GPL-3.0+ work (`LICENSE:1-3`) | Own code — no external obligation. |

### 3.2 Qt

| Dep | Where shipped / evidence | License | GPL-3.0 verdict |
|---|---|---|---|
| Qt 6.11.1 runtime (core, gui, quick, quickcontrols2, network, svg, QML modules) — `app/app.pro:1`; version pinned `build-win-mac.yml:24,27`; Windows deploy `scripts/build-arch.bat:256`; macOS frameworks `app/app.pro:588-593` + `scripts/generate-dmg.sh:69`; AppImage builds against apt Qt6 (`build-appimage.yml:32`) and linuxdeploy bundles it (`--plugin qt`, `scripts/build-appimage.sh:69`) | **UNKNOWN from repo** — no Qt license text is in-tree (glob for `LICENSE*` found none for Qt; windeployqt/macdeployqt output is not in the repo). Publicly Qt is LGPL-3.0/commercial dual-licensed — a verification pointer, not repo evidence. | **EXPECTED COMPATIBLE (LGPL-3.0 branch)** but unproven. Verify against the license files accompanying the actually deployed Qt copies. If LGPL: the dynamic DLL/dylib deployment used here satisfies it provided the license text is retained (currently not — action 2). Steam Link caveat: Qt comes from the SDK sysroot there (`scripts/build-steamlink-app.sh:39`); if that Qt is a *static* build, object-code/source obligations attach to that artifact (see §5). |

### 3.3 Prebuilt binaries from `moonlight-stream/moonlight-qt-deps` v12 (Windows, macOS, Steam Link)

Fetched by `setup-deps.ps1:3-7,17` (Windows) and `setup-deps.py:8-10,26` (macOS
`macos-universal.zip`; Steam Link `steamlink.zip`). **The archive contents are
not in this repo, so none of these licenses is provable in-tree; each is
`UNKNOWN` until the v12 archives and their build configs are inspected.** The
dependency list itself is proven by link lines:

| Dep | Evidence (link site) | License | GPL-3.0 verdict |
|---|---|---|---|
| OpenSSL (libssl/libcrypto; mac `-lssl.3` = OpenSSL 3.x) | `app/app.pro:77,163,171`; `moonlight-common-c.pro:37` | UNKNOWN (publicly Apache-2.0 for 3.x) | Conditional — Apache-2.0 is GPL-3.0-compatible; verify the actual archive. |
| SDL2 | `app/app.pro:163,171` | UNKNOWN (publicly zlib) | Expected compatible; verify. |
| SDL2_ttf | `app/app.pro:77,163,171` | UNKNOWN (publicly zlib; also embeds FreeType/HarfBuzz/Plutosvg whose licenses must be checked in the same archive) | Expected compatible; verify incl. embedded deps. |
| FFmpeg (avcodec/avutil/swscale; mac sonames `-lavcodec.61`-set at `:171`) | `app/app.pro:84-87,163,171`; DLLs shipped via `scripts/build-arch.bat:230` | UNKNOWN — neither version nor configure flags (LGPL vs GPL build) are visible from this repo | Conditional. Any GPL component would still be fine inside a GPL-3 app, but the license identity must be recorded. |
| libopus | `app/app.pro:81,163,171` | UNKNOWN (publicly BSD-3-Clause) | Expected compatible; verify. |
| libplacebo | `app/app.pro:140-143,163,171` | UNKNOWN (publicly MIT) | Expected compatible; verify. |
| discord-rpc | `app/app.pro:167,172,432-433`; no source in repo | UNKNOWN | Conditional — GPL-compatibility cannot be assumed until verified. |
| Steam Link extras: hand-optimized `libopus`, `libNE10`, `libarmasm` | `app/app.pro:388-389` | UNKNOWN | Conditional; verify — see §5. |

### 3.4 Steam Link SDK (Steam Link artifact only)

| Dep | Evidence | License | GPL-3.0 verdict |
|---|---|---|---|
| SLVideo / SLAudio (Valve Steam Link SDK; public repo `ValveSoftware/steamlink-sdk` checked out at `build-steamlink.yml:27-31,34-35`) | config test `config.tests/SL/SL.pro:2`, `config.tests/SL/main.cpp:1-2`; link `app/app.pro:392-393`; SDK env sourced `scripts/build-steamlink-app.sh:39` | **UNKNOWN — no license text in this repo** | **Cannot certify. See §5 — this gates the Steam Link artifact.** |

### 3.5 AppImage source-built libraries (bundled into the image)

Built from pinned upstream tags in CI and bundled by linuxdeploy
(`scripts/build-appimage.sh:67-69`); the upstreams' license files are not
vendored here, so licenses are `UNKNOWN` in-repo:

| Dep | Evidence (CI ref) | License | GPL-3.0 verdict |
|---|---|---|---|
| SDL3 (libsdl-org/SDL `release-3.4.14`) | `build-appimage.yml:42-54`; explicitly bundled `scripts/build-appimage.sh:68` | UNKNOWN (publicly zlib) | Expected compatible; verify. |
| sdl2-compat (`release-2.32.70`) | `build-appimage.yml:56-68` | UNKNOWN (publicly zlib) | Expected compatible; verify. |
| SDL_ttf (commit `a883e490…`) | `build-appimage.yml:70-83` | UNKNOWN (publicly zlib + embedded FreeType/HarfBuzz) | Expected compatible; verify embedded deps. |
| FFmpeg `n9.0`, built `--enable-libdav1d` with **no `--enable-gpl` flag** | `build-appimage.yml:129-147` (configure at `:139-146`) | UNKNOWN as SPDX; the visible config supports an LGPL-style build | Conditional; record the exact config in the notices. |
| dav1d 1.5.4, **static** into FFmpeg (`-Ddefault_library=static`) | `build-appimage.yml:117-127` | UNKNOWN (publicly BSD-2-Clause) | Expected compatible; static embedding raises dav1d notice + FFmpeg "materials to relink" obligations; verify. |
| libplacebo @`4d82c689…` **with our patch applied** | `build-appimage.yml:101-115` (patch applied at `:112`); patch `app/deploy/linux/appimage/libplacebo-disable-internally-synchronized-queues.patch:1-10` | UNKNOWN (publicly MIT) | Expected compatible. The patch lives in our GPL tree and must ship with corresponding source. |
| libva 2.24.1 | `build-appimage.yml:86-96`; feature gates `app/app.pro:89-101` | UNKNOWN (publicly MIT) | Expected compatible; verify. |

Note: the AppImage's exact bundle membership is whatever linuxdeploy's
dependency scan picks up at build time (`scripts/build-appimage.sh:67-69`) — it
is not enumerated in-repo. Treat that unknown bundle set itself as an audit
item (U7).

### 3.6 Microsoft Visual C++ runtime (Windows)

| Dep | Evidence | License | Verdict |
|---|---|---|---|
| VC++ 2015-2022 Redist installer | chained and **downloaded from Microsoft URLs at install time**, not embedded (`wix/MoonlightSetup/Bundle.wxs:2-13,63-101`, payload URLs at `:71-77,91-97`) | MS redistributable license — UNKNOWN in-repo | Download-not-embed keeps us out of redistribution for the installer path. |
| VC CRT DLLs in the **portable zip only** | `scripts/build-arch.bat:145,300-303` | UNKNOWN in-repo | MS redist terms generally permit CRT redistribution; confirm they cover loose DLLs in a third-party zip (U8). |

### 3.7 Bundled artwork and fonts (all platforms, embedded in the binary)

| Asset | Evidence | License | Verdict |
|---|---|---|---|
| `ModeSeven.ttf` font | embedded `app/qml.qrc:84` | **UNKNOWN** — no font license file in `app/` | **Cannot certify. Add the font's license or replace it (U5).** |
| Material Design / Material Symbols icons (`baseline-*.svg`, `*_FILL1_*.svg`, `settings.svg`, etc.) | embedded `app/resources.qrc:3-19` | UNKNOWN (publicly Apache-2.0) | Expected compatible; attribution currently absent; verify (U9). |
| `res/discord.svg` (Discord logo, UI button) | `app/resources.qrc:20` | **UNKNOWN — third-party trademark logo**, no in-repo grant | **Review against Discord's brand terms (U6).** |
| Own brand art (`moonlight.svg/png/ico`, `moonlight.icns`, `moonlight_wix.png`, `no_app_image.png`) | `app/resources.qrc:11-12`; `wix/MoonlightSetup/Bundle.wxs:55,58` | Part of our work | Fine. |
| AppData metadata | `app/deploy/linux/com.jochona.client.appdata.xml:4` `CC0-1.0` | Fine. |
| Translation catalogs `languages/*.ts/.qm` | `app/resources.qrc:21-81` | Community translations shipped inside the GPL work | Covered by the project license; no action. |

### 3.8 Build-time only — shipped in no artifact (no product obligations)

create-dmg (`scripts/generate-dmg.sh:1`), linuxdeploy + qt plugin
(`scripts/build-appimage.sh:15,67-69`; `build-appimage.yml:149-154`),
install-qt-action/aqtinstall (`build-win-mac.yml:49-56`), jom/nmake, 7-Zip,
signtool/symstore (`scripts/build-arch.bat:186-205,274-297`), Vulkan SDK
packages (`build-appimage.yml:29-36`), `git-archive-all.sh` itself (GPL-3.0+,
`scripts/git-archive-all.sh:23-24`). OS system libraries (Windows
`ws2_32/dxva2/d3d9/...` `app/app.pro:64`; macOS `-framework VideoToolbox` etc.
`app/app.pro:175`; X11/Wayland at runtime `app/app.pro:147-158`) are not
bundled by us.

## 4. Obligations triggered by distributing the packaged builds

1. **GPL-3.0 corresponding source (all artifacts).** Every shipped binary
   statically links the GPL moonlight-common-c (`app/app.pro:505-507`), so the
   full corresponding source = the Jochona tree **plus** the three submodules at
   their pinned commits **plus** vendored `h264bitstream/` and `AntiHooking/`
   **plus** build inputs (scripts, the libplacebo patch, exact FFmpeg config).
   The tool exists (`scripts/generate-src.sh:18`; submodules included per
   `scripts/git-archive-all.sh:250-262`) but nothing in the MSI, DMG, AppImage,
   or zips says where to get that source. The GPL §6 written offer / public
   source pointer is a real, currently unmet obligation on every artifact.
2. **Notice retention (MIT/zlib/BSD components).** enet, nanors, qmdnsengine,
   SDL_GameControllerDB (and, once verified, opus/SDL/dav1d/libplacebo/
   discord-rpc/OpenSSL) require their copyright + permission text to travel
   with the distribution. None of these texts appears in any shipped artifact:
   the MSI harvests only the deploy dir (`wix/Moonlight/Product.wxs:102-104`,
   contents per `scripts/build-arch.bat:230-238`), the DMG ships only the app
   bundle (`scripts/generate-dmg.sh:69-99`), and the Windows bootstrapper shows
   only the GPL (`Bundle.wxs:54`).
3. **LGPL components (Qt; FFmpeg if LGPL; h264bitstream if LGPL).** Dynamic
   deployment on Windows (`scripts/build-arch.bat:256`) and macOS
   (`app/app.pro:588-593`) keeps relinking rights intact; retain their license
   texts (same notices file as item 2). The AppImage bundles everything into
   one file but keeps FFmpeg shared (`--enable-shared`,
   `build-appimage.yml:139`); dav1d's static embedding should be handled per
   the LGPL "works that use the library" materials clause — satisfied by the
   published source tarball from item 1. **h264bitstream is the exception: it
   is statically linked and its LICENSE file is the bare LGPL-2.1 text**
   (`h264bitstream/LICENSE:1-2`, `h264bitstream.pro:13-14`) — resolve before
   relying on it (§3.1, U4).
4. **Windows specifics.** The bootstrapper already displays the full GPL text
   (`license.rtf:4-5`) — keep it in sync with the app license. The MSI and the
   portable zip need the notices file + source pointer (items 1-2). The VC++
   redist is downloaded, not embedded (`Bundle.wxs:63-101`); the only
   redistribution act is the CRT DLLs inside the *portable* zip
   (`scripts/build-arch.bat:300-303`) — confirm the current MS redist license
   permits it.
5. **macOS specifics.** No license text at all inside `Jochona.app` (bundle
   data is plist + icns only, `app/app.pro:579-585`). Add
   `Contents/Resources/LICENSE` + `Contents/Resources/THIRD-PARTY-NOTICES`.
6. **AppImage specifics.** A distributed AppImage must have its corresponding
   source published at/next to the download (item 1), its bundle membership
   enumerated (U7), and the bundled license texts included.

## 5. ⚠️ Steam Link SDK — loud flag

- The zip we ship contains **no SDK files** — only our binary plus our own
  metadata (`scripts/build-steamlink-app.sh:51-56`; CI uploads exactly that,
  `build-steamlink.yml:37-42`). Good.
- **But the shipped binary statically links SDK-supplied code**: `-lSLVideo
  -lSLAudio` (`app/app.pro:392-393`) and `-lopus -larmasm -lNE10`
  (`app/app.pro:388-389`). The SDK is pulled from the public
  `ValveSoftware/steamlink-sdk` checkout (`build-steamlink.yml:30`); **its
  license text is not in this repo — UNKNOWN.** Until a human reads the SDK
  repo's license and confirms that distributing a GPL binary which statically
  links SLVideo/SLAudio/NE10 to Steam Link users is permitted, the Steam Link
  zip is **not cleared for distribution**. Also determine whether the SDK's Qt
  is a static build — static LGPL Qt inside the binary triggers object-file /
  source obligations for Qt itself on this artifact only.
- The NE10/armasm/opus statics come from `steamlink.zip` of moonlight-qt-deps
  v12 (`setup-deps.py:8-10,26`) — a black box today; identify their upstreams
  and licenses.

## 6. UNKNOWN registry

| # | Unknown | Why it matters |
|---|---|---|
| U1 | Steam Link SDK license (SLVideo/SLAudio) + SDK Qt static/dynamic — **externally verified 2026-08-27 (§8): the SDK repo contains no license file at all** | Gates the Steam Link zip (§5); remains NOT CLEARED for public distribution |
| U2 | Every license inside the moonlight-qt-deps v12 archives (Win/mac/SL): OpenSSL, FFmpeg (+config), SDL2, SDL2_ttf (+embedded deps), opus, libplacebo, discord-rpc, NE10/armasm/opus statics | Shipped in MSI/DMG/portable zip or linked into binaries (§3.3) |
| U3 | Qt 6.11.1 license text as actually deployed by windeployqt / macdeployqt / apt Qt6 | LGPL compliance proof (§3.2) |
| U4 | h264bitstream upstream provenance — **RESOLVED 2026-08-27 (§8): LGPL-2.1-or-later, compatible** | Residual: vendored copy stripped per-file license headers — restore them |
| U5 | `ModeSeven.ttf` license | Shipped in every binary (§3.7) |
| U6 | `discord.svg` usage rights (Discord brand terms) | Shipped in every binary (§3.7) |
| U7 | AppImage CI source-built deps' license texts + final linuxdeploy bundle membership | AppImage notices (§3.5) |
| U8 | MS VC redist license coverage for loose CRT DLLs in the portable zip | Portable zip only (§3.6) |
| U9 | Material icon SVGs attribution requirement | Every binary (§3.7) |

## 7. Actions (ordered, real gaps only)

1. **Before distributing any Steam Link build**: read the
   `ValveSoftware/steamlink-sdk` license and the `steamlink.zip` v12 contents;
   confirm static linking of SLVideo/SLAudio/NE10 into a redistributed GPL
   binary is permitted, and whether SDK Qt is static (U1).
2. **Ship a `THIRD-PARTY-NOTICES` file + GPL source pointer in every
   artifact**: MSI payload (added to the deploy dir before the
   `Product.wxs:103` harvest), portable zip,
   `Jochona.app/Contents/Resources/`, AppImage. Covers MIT/zlib retention and
   the Qt/FFmpeg texts (U2, U3).
3. **Audit moonlight-qt-deps v12** — record versions, licenses, and FFmpeg
   configure flags (especially any GPL components) and pin the findings here
   (U2).
4. **Publish corresponding source for each release** (the
   `scripts/generate-src.sh` tarball already includes submodules) and link it
   from every artifact/installer page — the GPL §6 offer is currently satisfied
   by nothing shipped.
5. **Resolve h264bitstream**: obtain the upstream license statement (file says
   LGPL-2.1, source files carry no notices); if LGPL-2.1-only, get
   "or later"/permissive confirmation or replace the vendored copy (U4).
6. **License or replace `ModeSeven.ttf`**; confirm Discord logo usage for
   `discord.svg`; add Material icons attribution (U5, U6, U9).
7. **Confirm portable-zip CRT DLL redistribution** matches the current MS
   redist license terms (U8).

## 8. Follow-up verification (external sources, 2026-08-27, by Main)

Two UNKNOWNs resolved against upstream sources directly (outside the in-repo evidence rule):

1. **U4 resolved — h264bitstream is LGPL-2.1-or-later.** Upstream `aizvorski/h264bitstream` header block (`h264_stream.h`): "either version 2.1 of the License, or (at your option) any later version." LGPL-2.1-or-later permits relicensing under LGPL-3.0, whose terms permit static combination with a GPL-3.0 program provided relink+source conditions ride along — satisfied here by the GPL source release. Compatible. **Residual defect:** the vendored `h264bitstream/` copy in this repo deleted upstream's per-file license headers while keeping only the bare LICENSE text — restore the upstream headers (notice retention).
2. **U1 sharpened — Steam Link SDK has no license grant.** The public `ValveSoftware/steamlink-sdk` repository contains no LICENSE/COPYING file in its root; the README describes building and deploying applications but makes no license statement. The static-link question for `SLVideo`/`SLAudio` cannot be answered from documents. Practical mitigation already in place: the artifact ships no SDK files, and upstream Moonlight has distributed the same static build for years with Valve's knowledge (the SDK exists for third-party apps). Before Jochona's first public release: get a one-line confirmation from Valve or gate the Steam Link artifact — never let the release pipeline auto-publish it unconfirmed.
3. Everything else (U2, U3, U5–U9) remains open; the v12 deps archives audit (action 3) is next-highest value and fully automatable.
