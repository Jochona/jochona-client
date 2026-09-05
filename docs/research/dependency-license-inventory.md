# Dependency & License Inventory — Jochona Client

Date: 2026-08-30 (packaging/notices closure pass; supersedes the 2026-08-27
audit below it). Scope: everything the build/packaging pipeline compiles in
or ships in a distributable artifact. Evidence rule: every license claim
cites either a path in this repo, or — where the repo cannot prove a
license because the archive/repo in question isn't fetchable from here —
an explicitly marked external primary-source citation (upstream LICENSE/
COPYING file or the component's own compiled-in license string), following
the same methodology used for U1/U4 in the 2026-08-27 pass (§8). Nothing in
this document is filled in from memory or from "publicly reported"
descriptions alone; every non-repo claim below names the exact upstream
file or compiled string it was checked against, and the date it was
checked. **Where neither an in-repo nor an external primary source is
reachable, the entry is still marked `UNKNOWN`.**

This file is the audit that `README.md` points at ("see
`docs/research/dependency-license-inventory.md` for the full
dependency/license audit").

## 0. What changed in this pass

1. Every shipped artifact now carries a third-party notices bundle: full
   verbatim license texts for every dependency whose license is known,
   this project's own GPL-3.0 `LICENSE`, and a corresponding-source
   pointer. See §2 and §9.
2. `libplacebo`'s license was **corrected**: the 2026-08-27 pass recorded
   "publicly MIT" as an unverified placeholder; direct inspection of the
   actual moonlight-qt-deps v12 archive contents shows libplacebo is
   **LGPL-2.1-or-later**. See §3.3, §9.
3. FFmpeg's license is now **proven, not assumed**, for both the
   Windows/macOS prebuilt archive and the Linux AppImage source build, by
   reading the actual configure invocation and license string compiled
   into the shipped binaries. See §3.3, §3.5, §9.
4. Qt, OpenSSL, SDL2/SDL3/sdl2-compat, SDL_ttf, opus, dav1d, libva, and
   discord-rpc all moved from `UNKNOWN` to a cited license. See §3.2, §3.3,
   §3.5, §9.
5. h264bitstream's stripped per-file license headers are restored
   verbatim from upstream. See §3.1, §9.
6. Material icon SVGs now carry Apache-2.0 attribution, embedded in the
   binary. See §3.7, §9.
7. Steam Link publication is now hard-gated in both the build script and
   CI workflow behind an explicit human confirmation that does not exist
   yet, so it cannot auto-publish. See §5, §9.
8. The Microsoft VC++ Redistributable question (U8) is resolved: Microsoft's
   own documentation permits redistributing the unmodified merge-module
   CRT DLLs from a licensed Visual Studio install, which is exactly what
   the portable zip does. See §3.6, §9.
9. A previously undocumented, already-compliant asset (the CC0 Kenney
   input-prompt icon fonts) is now recorded in §3.7 so it doesn't get
   mistaken for an open gap.

Real remaining gaps after this pass (see §6, §7): the Steam Link SDK's
missing license grant (U1, hard-gated and not fixable from this repository)
and the exact final AppImage bundle membership produced by linuxdeploy.

## 1. The Jochona work itself

| Item | License | Evidence |
|---|---|---|
| App code (`app/`, `AntiHooking/`, `masterhook*.c`, `h264bitstream/` vendored dir, scripts) | GPL-3.0-or-later (declared) | Full GPL-3.0 text at `LICENSE:1-3`; `README.md` "GPL-3.0, same as upstream"; `app/deploy/linux/com.jochona.client.appdata.xml:5` `<project_license>GPL-3.0+</project_license>` |

No source file in `app/` or `AntiHooking/` carries an individual license header
(grep for GPL / "at your option" notices in `app/main.cpp`,
`AntiHooking/antihookingprotection.cpp` found none); the whole-tree LICENSE file
plus README/appdata declarations are the only license statements for the app.
`h264bitstream/` now carries restored per-file LGPL-2.1-or-later headers (§3.1) —
this doesn't change the app's own GPL declaration, it's the vendored dependency's
own notice.

Submodules are pinned in `.gitmodules`: `moonlight-common-c` (`.gitmodules:1-2`),
`qmdnsengine` (`.gitmodules:3-4`), `SDL_GameControllerDB` (`.gitmodules:5-6`).
`h264bitstream/` and `AntiHooking/` are **vendored** sources tracked directly in
this repo, not submodules.

## 2. What each shipped artifact contains

Every artifact below ships `THIRD-PARTY-NOTICES.txt`, a complete
`licenses/` directory for long-form LGPL/Apache/OFL/FreeType/HarfBuzz texts,
the project `LICENSE` as `LICENSE.txt`, `SOURCE-POINTER.txt`, and a
`VERSION.txt` recording the exact build.

| Artifact | Contents (proven by) | Notices added by |
|---|---|---|
| Windows `JochonaSetup-<ver>.exe` bundle (built `scripts/generate-bundle.bat:77,81`) | Bootstrapper (displays GPL text via `wix/MoonlightSetup/Bundle.wxs:54` → `wix/MoonlightSetup/license.rtf:4-7`, now including a corresponding-source pointer paragraph), VC++ Redist **downloaded from Microsoft at install time**, not embedded (`Bundle.wxs:2-13,63-101`), and two per-arch MSIs (`Bundle.wxs:103-116`) — each of which carries the notices bundle (see next row). | Bootstrapper payload itself carries no separate deploy dir; notices reach it transitively via the two `MsiPackage`s it chains, plus the license.rtf pointer paragraph. |
| Windows `Jochona.msi` | `Jochona.exe` plus harvest of the whole deploy dir (`wix/Moonlight/Product.wxs:108`, `Files Include="$(var.DeployDir)\**"`). Deploy dir = prebuilt-lib DLLs (`scripts/build-arch.bat:230`), `AntiHooking.dll` (`:233-235`), `gamecontrollerdb.txt` (`:238`), Qt runtime + QML modules via windeployqt (`:266`; `icuuc.dll` deleted at `:282`). | `scripts/build-arch.bat:241-249` copies `LICENSE`, `THIRD-PARTY-NOTICES.txt`, `SOURCE-POINTER.txt`, `VERSION.txt` into `%DEPLOY_FOLDER%` before the MSI harvest, so WiX's `**` glob picks them up automatically. |
| Windows portable `JochonaPortable-<arch>-<ver>.zip` (`scripts/build-arch.bat:328`) | Same deploy dir **plus VC CRT DLLs copied out of Visual Studio** (`scripts/build-arch.bat:145,310-313`). | Same deploy-dir copy as the MSI row (the zip is `7z a ... %DEPLOY_FOLDER%\*`, built after the notices copy). |
| macOS `Jochona-<ver>.dmg` (`scripts/generate-dmg.sh:87-97,99,107`) | `Jochona.app` after macdeployqt (`:69`) — embeds Qt frameworks plus everything from `libs/mac` (`app/app.pro:641-645`). | `scripts/generate-dmg.sh:74-80` copies `LICENSE`, `THIRD-PARTY-NOTICES.txt`, `SOURCE-POINTER.txt`, `VERSION.txt` into `Jochona.app/Contents/Resources/` before `codesign --deep` runs, so they're inside the signed bundle. |
| Linux AppImage (`scripts/build-appimage.sh:75-77`) | App plus linuxdeploy-qt-plugin bundle and `libSDL3.so.0` (`:76`); builds against apt Qt6 (`build-appimage.yml:32`). CI builds SDL3, sdl2-compat, SDL_ttf, libva, patched libplacebo, dav1d, FFmpeg from source into the image (`build-appimage.yml:42-147`). | `scripts/build-appimage.sh:62-68` copies the notices bundle into `$DEPLOY_FOLDER/usr/share/doc/jochona/` before `make install`'s AppDir is handed to `linuxdeploy`, so it's bundled into the `.AppImage`. |
| Steam Link `Jochona-SteamLink-<ver>.zip` (`scripts/build-steamlink-app.sh:73`) | The `Jochona` binary (`:66`) plus `app/deploy/steamlink/` metadata (`:67`) and the notices bundle. **Gated — see §5.** | `scripts/build-steamlink-app.sh:68-71` copies the notices bundle alongside the binary. The whole script now refuses to run (`scripts/build-steamlink-app.sh:14-25`) unless `STEAMLINK_LICENSE_CONFIRMED=1` is explicitly set, and CI (`.github/workflows/build-steamlink.yml:27-49`) skips the build/upload steps entirely unless the `STEAMLINK_LICENSE_CONFIRMED` repository variable is `'true'` — which it is not, so this artifact currently cannot be produced by CI at all. |
| Source tarball `JochonaSrc-<ver>.tar.gz` (`scripts/generate-src.sh:20`) | Superproject **and all submodules** via `scripts/git-archive-all.sh` (`:6-8,250-262`). |  |

## 3. Direct build/runtime dependencies

### 3.1 In-tree and submodule sources (statically linked into every binary)

| Dep | Link mode / evidence | License (proof) | GPL-3.0 verdict |
|---|---|---|---|
| moonlight-common-c (submodule) | static lib (`moonlight-common-c/moonlight-common-c.pro:12-13`), linked `app/app.pro:547-549` | GPL-3.0 — verbatim text at `moonlight-common-c/moonlight-common-c/LICENSE.txt:1-2`; no per-file "or later" notice found → treat as GPL-3.0-only | **Compatible** (GPL + GPL); app must be conveyed under GPL terms — already the case. |
| enet (inside the moonlight-common-c checkout) | compiled into the static lib (`moonlight-common-c.pro:44-52`) | MIT — `moonlight-common-c/moonlight-common-c/enet/LICENSE:1-3` (Lee Salzman 2002-2020) | **Compatible.** MIT notice retention now satisfied: full text in `app/deploy/notices/THIRD-PARTY-NOTICES.txt` §9, shipped in every artifact (§2). |
| nanors (inside the moonlight-common-c checkout) | compiled in (`moonlight-common-c.pro:53-55`) | MIT — `moonlight-common-c/moonlight-common-c/nanors/LICENSE:1-3` (Joseph Calderon 2021) | **Compatible.** Notice retention satisfied (see above). |
| qmdnsengine (submodule) | static lib (`qmdnsengine/qmdnsengine.pro:7-8`), linked `app/app.pro:554-556` | MIT — `qmdnsengine/qmdnsengine/LICENSE.txt:3,5` (Nathan Osman 2018) | **Compatible.** Notice retention satisfied. |
| h264bitstream (vendored) | static lib (`h264bitstream/h264bitstream.pro:13-14`), linked `app/app.pro:561-563` | **RESOLVED 2026-08-27, headers restored 2026-08-30: LGPL-2.1-or-later.** Upstream `aizvorski/h264bitstream` copyright block ("Copyright (C) 2005-2007 Auroras Entertainment, LLC; Copyright (C) 2008-2011 Avail-TVN") and "or (at your option) any later version" language now restored verbatim atop `h264_stream.h`, `h264_stream.c`, `h264_nal.c`, and `bs.h`; full LGPL-2.1 text still at `h264bitstream/LICENSE:1-2`. | **Compatible.** LGPL-2.1-or-later + static linking into GPL-3.0+ is fine once "or later"/LGPL-3.0 is confirmed selectable — it is (see upstream header). Full text shipped in every artifact's notices bundle (§2, §9). |
| SDL_GameControllerDB (submodule; data file) | `gamecontrollerdb.txt` embedded in the exe (`app/qml.qrc` and `app/resources.qrc:83`) and copied beside the exe on Windows (`scripts/build-arch.bat:238`) | zlib-style SDL license — `app/SDL_GameControllerDB/LICENSE:1-9` (Sam Lantinga 1997-2025) | **Compatible.** Notice retention **resolved**: full text now in `THIRD-PARTY-NOTICES.txt`, shipped in every artifact (§2). |
| AntiHooking | vendored in-tree, linked `app/app.pro:569-570`; shipped as Windows DLL (`scripts/build-arch.bat:233-235`) | Part of the GPL-3.0+ work (`LICENSE:1-3`) | Own code — no external obligation. |

### 3.2 Qt

| Dep | Where shipped / evidence | License | GPL-3.0 verdict |
|---|---|---|---|
| Qt 6.11.1 runtime (core, gui, quick, quickcontrols2, network, svg, sql, QML modules) — `app/app.pro:1`; version pinned `build-win-mac.yml:24,27`, `build.yml:34`; Windows deploy `scripts/build-arch.bat:266`; macOS frameworks `app/app.pro:641-645` + `scripts/generate-dmg.sh:69`; AppImage builds against apt Qt6 (`build-appimage.yml:32`) and linuxdeploy bundles it (`--plugin qt`, `scripts/build-appimage.sh:76`) | **RESOLVED 2026-08-30: LGPL-3.0-only (open-source edition).** No in-tree Qt LICENSE text exists (windeployqt/macdeployqt output isn't committed), so this is an external-tooling-behavior verification rather than an in-repo file: every CI workflow installs Qt exclusively via `jurplel/install-qt-action`/`aqtinstall` (`build-win-mac.yml:48-66`, `build.yml:32-35`, `i18n-regen.yml`), which only ever downloads from Qt's official open-source package repository; a full grep of `.github/workflows/*.yml` for any commercial-Qt indicator (license key, Qt account, commercial installer) found none. Dynamic linking on every platform (windeployqt, macdeployqt, `linuxdeploy --plugin qt`) preserves LGPL-3.0 relinking rights. | **Compatible (confirmed).** Full LGPL-3.0 text (which incorporates GPL-3.0 by reference to this project's own `LICENSE`) shipped in every artifact's notices bundle (§2, §9). |

### 3.3 Prebuilt binaries from `moonlight-stream/moonlight-qt-deps` v12 (Windows, macOS, Steam Link)

Fetched by `setup-deps.ps1:3-7,17` (Windows) and `setup-deps.py:8-10,26`
(macOS `macos-universal.zip`; Steam Link `steamlink.zip`). This pass inspected
the extracted macOS tree plus the published v12 `windows-x64.zip`,
`windows-ARM64.zip`, and `steamlink.zip` archives directly. Both Windows
architectures contain the same OpenSSL 3.6.3, SDL license headers,
libplacebo 7.371 LGPL header, runtime-library versions, and FFmpeg
`LGPL version 2.1 or later` binary license string as the macOS archive.
Their embedded FFmpeg configure commands contain no `--enable-gpl`,
`--enable-nonfree`, or `--enable-version3`. The Steam Link archive contains
Opus 1.5.2 code (`libopus.a` and `libarmasm.a`) plus Project Ne10
(`libNE10.a`); its only unresolved license surface is the separate Valve SDK.

| Dep | Evidence (link site) | License | GPL-3.0 verdict |
|---|---|---|---|
| OpenSSL (libssl/libcrypto; mac `-lssl.3 -lcrypto.3` `app/app.pro:177`, win `-llibssl -llibcrypto` `app/app.pro:169`) | `app/app.pro:83,169,177`; `moonlight-common-c.pro:37` | **RESOLVED 2026-08-30: Apache License 2.0, version 3.6.3.** `libs/mac/include/openssl/opensslv.h`: `OPENSSL_VERSION_STR "3.6.3"`. OpenSSL 3.x is Apache-2.0 (verified against the upstream `openssl/openssl` `LICENSE.txt`, which is the standard Apache-2.0 text) — no 1.x/OpenSSL-license dual-licensed copy is present. | **Compatible (confirmed).** Apache-2.0 text shipped in the notices bundle (§9). |
| SDL2 / SDL3 / sdl2-compat | `app/app.pro:83,169,177` | **RESOLVED 2026-08-30: zlib.** `libs/mac/include/SDL2/SDL_copying.h` and `libs/mac/include/SDL3/SDL_copying.h` carry the full zlib license text in-tree (Copyright (C) 1997-2026 Sam Lantinga). SDL3 header reports version 3.4.14 (`SDL_MAJOR/MINOR/MICRO_VERSION`), matching the AppImage's own `release-3.4.14` pin (`build-appimage.yml:46`) — the same upstream tag is used for both the prebuilt archive and the from-source AppImage build. The macOS SDL2 dylib's own exact release tag is not recorded in the shipped headers. | **Compatible (confirmed).** zlib text shipped in the notices bundle. |
| SDL2_ttf | `app/app.pro:83,169,177` | **RESOLVED 2026-08-30: zlib for SDL_ttf itself**, version 2.25. The v12 source pins SDL_ttf `a883e490e30fb44a5336ea3dcb990c6982c5216f` with vendored FreeType `535d2993d2cab67cc8b270132b1b1bcb62f269aa` and HarfBuzz `950d232cdcd29e2e83a2099307e5624a8f1aa937`. FreeType is conveyed under the FreeType License; HarfBuzz uses Old MIT. | **Compatible (confirmed).** SDL zlib, FreeType, and HarfBuzz texts are bundled under `app/deploy/notices/licenses/`. |
| FFmpeg (avcodec/avutil/swscale; mac sonames `-lavcodec.63 -lavutil.61 -lswscale.10` at `app/app.pro:177`) | `app/app.pro:92,169,177`; DLLs shipped via `scripts/build-arch.bat:230` | **RESOLVED 2026-08-30: LGPL-2.1-or-later, no GPL/nonfree components.** Read directly from the shipped `libavcodec.63.dylib`: (1) the binary's own embedded license string (what `avcodec_license()` returns) is exactly `"LGPL version 2.1 or later"`; (2) the configure command baked into the binary (`avcodec_configuration()`) is `--prefix=.../build_arm64 --extra-cflags='-mmacosx-version-min=13.0 -arch arm64 ...' --cc=clang --arch=arm64 --enable-cross-compile --enable-pic --enable-lto --fatal-warnings --enable-shared --disable-static --disable-all --disable-autodetect --enable-avcodec --enable-avformat --enable-swscale --enable-vulkan --enable-decoder=h264 --enable-decoder=hevc --enable-decoder=av1 --enable-videotoolbox --enable-hwaccel=h264_videotoolbox --enable-hwaccel=hevc_videotoolbox --enable-hwaccel=av1_videotoolbox --enable-libdav1d --enable-decoder=libdav1d` (x86_64 build identical apart from arch flags) — no `--enable-gpl`, `--enable-nonfree`, or `--enable-version3`, and no GPL-only codec (x264/x265/etc.) enabled; (3) the build id string (`FFMPEG_VERSION` / `ffversion.h`) is `"d32b387"` (moonlight-qt-deps' own FFmpeg fork/mirror commit, not a mainline tag). dav1d (BSD-2-Clause, see below) is linked in with no separate `libdav1d.dylib`, i.e. statically embedded even in this LGPL build. | **Compatible (confirmed).** LGPL-2.1-or-later text shipped in the notices bundle; exact configure flags recorded verbatim there for future audits. |
| libopus | `app/app.pro:87,169,177` | **RESOLVED 2026-08-30: BSD-3-Clause (IETF non-endorsement variant).** `libs/mac/include/opus.h` header carries a partial (2-clause) copy of the same text; the full 3-clause text was checked against the upstream `xiph/opus` `COPYING` file (Copyright 2001-2023 Xiph.Org, Skype Limited, Octasic, and others) on 2026-08-30. Exact opus release is not recorded in the shipped headers/binary (`libopus.0.dylib`, ABI SONAME `0`, `opus_get_version_string()` not statically greppable from the stripped binary). | **Compatible (confirmed).** Full text shipped in the notices bundle. |
| libplacebo | `app/app.pro:146-148,169,177` | **CORRECTED 2026-08-30: LGPL-2.1-or-later, not MIT.** The 2026-08-27 pass recorded "publicly MIT" as an unverified placeholder. Direct inspection of `libs/mac/include/libplacebo/config.h` shows the actual license header: "libplacebo is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License ... version 2.1 of the License, or (at your option) any later version." The compiled `libplacebo.dylib` reports build string `v7.371.0` (`PL_MAJOR_VER=7`, `PL_API_VER=371`). Dynamically linked, preserving relinking rights. | **Compatible (confirmed, correcting the prior placeholder).** LGPL-2.1-or-later text shipped in the notices bundle; the AppImage's patch (`app/deploy/linux/appimage/libplacebo-disable-internally-synchronized-queues.patch`) lives in this GPL tree and ships with corresponding source per `SOURCE-POINTER.txt`. |
| discord-rpc | `app/app.pro:173,178,470-473`; no source in repo | **RESOLVED 2026-08-30 (external verification, same methodology as U1/U4): MIT, Copyright 2017 Discord, Inc.** No LICENSE ships in this archive, so the upstream `discordapp/discord-rpc` repository's `LICENSE` file was read directly on 2026-08-30; full text reproduced in the notices bundle. | **Compatible (confirmed).** MIT text shipped in the notices bundle. |
| Steam Link extras: hand-optimized `libopus`, `libNE10`, `libarmasm` | `app/app.pro:429` (`config_SL { ... LIBS += -lopus -larmasm -lNE10 }`) | **RESOLVED 2026-08-30.** Direct inspection of v12 `steamlink.zip` identifies `libopus.a` and `libarmasm.a` as Opus 1.5.2 code (the archive embeds the `opus-1.5.2/celt/arm/celt_pitch_xcorr_arm-gnu.S` source path), covered by Opus's BSD-3-Clause license. `libNE10.a` is Project Ne10, BSD-3-Clause, Copyright 2012-16 ARM Limited and Contributors. | **Compatible (confirmed).** Both full texts are reproduced in the notices bundle. The separate Valve SDK grant still gates the complete artifact (§5). |

### 3.4 Steam Link SDK (Steam Link artifact only)

| Dep | Evidence | License | GPL-3.0 verdict |
|---|---|---|---|
| SLVideo / SLAudio (Valve Steam Link SDK; public repo `ValveSoftware/steamlink-sdk` checked out at `build-steamlink.yml:27-33`, only when the license gate below is open) | config test `config.tests/SL/SL.pro:2`, `config.tests/SL/main.cpp:1-2`; link `app/app.pro:433` | **UNKNOWN — no license text in this repo or upstream.** | **Cannot certify — see §5.** This is now a hard build/CI gate (§2, §9), not just a documentation warning: `scripts/build-steamlink-app.sh` refuses to build and the CI workflow skips the Steam Link job entirely without an explicit human-set confirmation that does not exist today. |

### 3.5 AppImage source-built libraries (bundled into the image)

Built from pinned upstream tags in CI and bundled by linuxdeploy
(`scripts/build-appimage.sh:75-77`). This pass verified each license against
the pinned tag's own upstream LICENSE/COPYING file (external verification,
methodology matching U1/U4/§8) since those texts aren't vendored in this repo.

| Dep | Evidence (CI ref) | License | GPL-3.0 verdict |
|---|---|---|---|
| SDL3 (libsdl-org/SDL `release-3.4.14`) | `build-appimage.yml:42-54`; explicitly bundled `scripts/build-appimage.sh:76` | **RESOLVED: zlib** — same text as §3.3's in-tree SDL_copying.h evidence (the AppImage and the moonlight-qt-deps prebuilt pin the identical `release-3.4.14` tag). | Expected compatible; confirmed. |
| sdl2-compat (`release-2.32.70`) | `build-appimage.yml:56-68` | **RESOLVED: zlib** — libsdl-org/sdl2-compat carries the same SDL zlib license family; `SDL_REVISION` string in the moonlight-qt-deps mac archive (`"SDL-2.32.70-..."`) confirms the same tag is used across platforms for the SDL2-compat layer. | Expected compatible; confirmed. |
| SDL_ttf (commit `a883e490…`) | `build-appimage.yml:70-83` | **Resolved:** zlib for SDL_ttf, with recursive FreeType commit `535d2993…` under the FreeType License and HarfBuzz commit `950d232c…` under Old MIT. The workflow installs system FreeType development headers and also checks out the pinned recursive sources; either permitted build path is covered by the bundled license texts. | Compatible; complete texts ship in the notices license directory. |
| FFmpeg `n9.0`, built `--enable-libdav1d` with **no `--enable-gpl` flag** | `build-appimage.yml:129-147` (configure at `:139-147`) | **RESOLVED: LGPL-2.1-or-later**, matching §3.3's FFmpeg finding. The exact configure line, quoted directly from `build-appimage.yml:139-147`, has no `--enable-gpl`/`--enable-nonfree`/`--enable-version3`, consistent with the license string embedded in the equivalent macOS build. | Compatible; confirmed. Full configure line recorded verbatim in the notices bundle. |
| dav1d 1.5.4, **static** into FFmpeg (`-Ddefault_library=static`) | `build-appimage.yml:117-127` | **RESOLVED: BSD-2-Clause.** Upstream `videolan/dav1d` `COPYING` (Copyright © 2018-2025, VideoLAN and dav1d authors) checked directly on 2026-08-30. | Expected compatible; confirmed. Static embedding raises dav1d notice + FFmpeg "materials to relink" obligations, satisfied by the published corresponding source (SOURCE-POINTER.txt / GPL §6). |
| libplacebo @`4d82c689…` **with our patch applied** | `build-appimage.yml:101-115` (patch applied at `:112`); patch `app/deploy/linux/appimage/libplacebo-disable-internally-synchronized-queues.patch:1-10` | **CORRECTED: LGPL-2.1-or-later**, matching §3.3's correction. | Compatible; confirmed. The patch lives in our GPL tree and ships with corresponding source. |
| libva 2.24.1 | `build-appimage.yml:86-96`; feature gates `app/app.pro:95-107` | **RESOLVED: MIT-style permissive license** (SGI/Precision-Insight style; upstream `intel/libva` `COPYING`, checked directly on 2026-08-30). | Expected compatible; confirmed. |

Note: the AppImage's exact bundle membership is whatever linuxdeploy's
dependency scan picks up at build time (`scripts/build-appimage.sh:75-77`) — it
is not enumerated in-repo, but every dependency it can pull from is now
license-identified above (U7 closed as far as licenses go; the *membership
enumeration* itself remains an open, lower-value follow-up — see §7).

### 3.6 Microsoft Visual C++ runtime (Windows)

| Dep | Evidence | License | Verdict |
|---|---|---|---|
| VC++ 2015-2022 Redist installer | chained and **downloaded from Microsoft URLs at install time**, not embedded (`wix/MoonlightSetup/Bundle.wxs:2-13,63-101`, payload URLs at `:71-77,91-97`) | MS redistributable license — UNKNOWN in-repo | Download-not-embed keeps us out of redistribution for the installer path. |
| VC CRT DLLs in the **portable zip only** | `scripts/build-arch.bat:145,310-313` | MS redist terms (external, not vendored) | **RESOLVED 2026-08-30 (U8).** Microsoft's own documentation (`learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files`, checked 2026-08-30) states redistribution of the unmodified VC++ Redistributable files — including merge-module DLLs — is permitted for licensed Visual Studio users, provided the files are copied unmodified. `build-arch.bat:145,310-313` copies the DLLs verbatim (`copy "%VC_REDIST_DLL_PATH%\*.dll"`) straight from the Visual Studio installation used to build (a licensed install, since it's what compiles the product), with no modification. This satisfies the redistribution terms as documented. |

### 3.7 Bundled artwork and fonts (all platforms, embedded in the binary)

| Asset | Evidence | License | Verdict |
|---|---|---|---|
| `ModeSeven.ttf` font | **REMOVED 2026-08-30.** The debug/stats overlay (`app/streaming/video/overlaymanager.cpp`) now loads the already-bundled, already-OFL-licensed `Space Grotesk` (`:/fonts/SpaceGrotesk-Regular.ttf`, see the `Space Grotesk` row below) instead; `app/ModeSeven.ttf` and its `resources.qrc` entry are deleted. | N/A — no longer shipped | **Resolved (U5).** |
| `Inter` (Regular/Medium/SemiBold/Bold static TTFs) | `app/fonts.qrc`; upstream `rsms/inter` v4.1 release | **SIL OFL 1.1** — text shipped `app/fonts/LICENSE-Inter.txt` + embedded in binary via `fonts.qrc` | Fine. OFL attribution satisfied by bundled license; family name unmodified. |
| `Space Grotesk` (Regular/Medium/Bold static TTFs) | `app/fonts.qrc`; upstream `floriankarsten/space-grotesk` 2.0.0 release | **SIL OFL 1.1** — text shipped `app/fonts/LICENSE-SpaceGrotesk.txt` + embedded | Fine. |
| `Noto Emoji` (variable COLRv1 TTF) | `app/fonts.qrc`; upstream `google/fonts` `ofl/notoemoji` | **SIL OFL 1.1** — text shipped `app/fonts/LICENSE-NotoEmoji.txt` + embedded | Fine. |
| Kenney input-prompt icon fonts (`kenney_input_xbox_series.ttf`, `kenney_input_playstation_series.ttf`, `kenney_input_nintendo_switch.ttf`, `kenney_input_steam_deck.ttf`, `kenney_input_generic.ttf`) | `app/fonts.qrc:11-16`; upstream Kenney "Input Prompts (1.5A)" pack | **CC0 1.0** (public domain dedication) — text shipped `app/fonts/LICENSE-Kenney.txt:10-11` + embedded | Fine, no attribution required. **Newly documented in this pass** — this asset and its license text were already correctly bundled by existing code (`fonts.qrc:16`) but had never been recorded in this audit; no code/packaging change was needed, just closing the documentation gap. |
| Brand masters `assets/brand/*` (icon, hero, banners, logo marks) | generated outputs committed; generated by `scripts/generate-brand-assets.py` | Commissioned/generated for this project — our work | Fine. |
| Material Design / Material Symbols icons (`baseline-*.svg`, `*_FILL1_*.svg`, `settings.svg`, `arrow_left.svg`, `question_mark.svg`, `update.svg`, etc.) | embedded `app/resources.qrc:3-19` | Apache License 2.0 — Copyright Google Inc. | **Resolved (U9) 2026-08-30.** Naming convention (`ic_*_white_48px`, `baseline-*-24px`, `*_FILL1_wght*_GRAD*_opsz*`) matches Google's Material Icons/Symbols export format exactly, checked against `google/material-design-icons`'s `LICENSE` (Apache-2.0) on 2026-08-30. Attribution text added at `app/res/LICENSE-MaterialIcons.txt` and embedded into the binary via `app/resources.qrc:20`; also reproduced in `THIRD-PARTY-NOTICES.txt`. |
| `res/discord.svg` (Discord logo) | **REMOVED 2026-08-30.** Confirmed unreferenced by any `Image`/icon source in `app/gui/**` (Discord Rich Presence is the unrelated `discord-rpc` library, §3.3, untouched); the SVG and its `resources.qrc` entry are deleted as dead weight, not a brand-terms clearance. | N/A — no longer shipped | **Resolved (U6).** |
| Own brand art (`Jochona.ico`, `Jochona.icns`, `jochona-wix.png`, `res/no_app_image.png`, `res/jochona-512.png`) | `app/app.pro:608,625`; `wix/MoonlightSetup/Bundle.wxs:55`; `app/resources.qrc:11,15` (corrected from a stale "moonlight.svg/png/ico, moonlight_wix.png" citation in the 2026-08-27 pass — those files were already renamed to the `Jochona`/`jochona-` names above by the time this pass ran) | Part of our work | Fine. |
| AppData metadata | `app/deploy/linux/com.jochona.client.appdata.xml:4` `CC0-1.0` | Fine. |
| Translation catalogs `languages/*.ts/.qm` | `app/resources.qrc:21-81` | Community translations shipped inside the GPL work | Covered by the project license; no action. |

### 3.8 Build-time only — shipped in no artifact (no product obligations)

create-dmg (`scripts/generate-dmg.sh:1`), linuxdeploy + qt plugin
(`scripts/build-appimage.sh:15,75-77`; `build-appimage.yml:149-154`),
install-qt-action/aqtinstall (`build-win-mac.yml:49-56`), jom/nmake, 7-Zip,
signtool/symstore (`scripts/build-arch.bat:107,208-217,284-299`), Vulkan SDK
packages (`build-appimage.yml:29-36`), `git-archive-all.sh` itself (GPL-3.0+,
`scripts/git-archive-all.sh:23-24`). OS system libraries (Windows
`ws2_32/dxva2/d3d9/...` `app/app.pro:65`; macOS `-framework VideoToolbox` etc.
`app/app.pro:181`; X11/Wayland at runtime `app/app.pro:154-165`) are not
bundled by us.

## 4. Obligations triggered by distributing the packaged builds

1. **GPL-3.0 corresponding source (all artifacts).** Every shipped binary
   statically links the GPL moonlight-common-c (`app/app.pro:547-549`), so the
   full corresponding source = the Jochona tree **plus** the three submodules at
   their pinned commits **plus** vendored `h264bitstream/` and `AntiHooking/`
   **plus** build inputs (scripts, the libplacebo patch, exact FFmpeg config).
   The tool exists (`scripts/generate-src.sh:18`; submodules included per
   `scripts/git-archive-all.sh:250-262`), and **every artifact now points at it**:
   `app/deploy/notices/SOURCE-POINTER.txt`, shipped in the MSI/portable zip
   (`scripts/build-arch.bat:241-249`), the DMG (`scripts/generate-dmg.sh:74-80`),
   the AppImage (`scripts/build-appimage.sh:62-68`), and — if it's ever unblocked
   — the Steam Link zip (`scripts/build-steamlink-app.sh:68-71`). The Windows
   bootstrapper's license.rtf also now names the source URL directly
   (`wix/MoonlightSetup/license.rtf:7`). **This closes the §6 obligation that was
   previously unmet on every artifact.**
2. **Notice retention (MIT/zlib/BSD/Apache components).** enet, nanors,
   qmdnsengine, SDL_GameControllerDB, OpenSSL, SDL2/SDL3/sdl2-compat, SDL_ttf,
   opus, dav1d, libva, discord-rpc, and the Material icons all require their
   copyright + permission text to travel with the distribution. **Resolved**:
   full texts for every one of these now live in
   `app/deploy/notices/THIRD-PARTY-NOTICES.txt`, shipped in every artifact per
   §2. (Material icons additionally get their own embedded copy at
   `app/res/LICENSE-MaterialIcons.txt` via `resources.qrc`, matching the
   existing font-license embedding pattern.)
3. **LGPL components (Qt; FFmpeg; libplacebo; h264bitstream).** Dynamic
   deployment on Windows (`scripts/build-arch.bat:266`) and macOS
   (`app/app.pro:641-645`) keeps relinking rights intact for Qt/FFmpeg/
   libplacebo; retain their license texts (now in the same notices file as
   item 2, §9). The AppImage bundles everything into one file but keeps FFmpeg
   shared (`--enable-shared`, `build-appimage.yml:139`); dav1d's and (on
   Windows/macOS) libplacebo's static embedding is handled per the LGPL "works
   that use the library" materials clause — satisfied by the published source
   tarball from item 1. **h264bitstream** is statically linked and both its
   `LICENSE` file (LGPL-2.1 text, `h264bitstream/LICENSE:1-2`) and its
   per-file headers (restored 2026-08-30, §9) are now present — **resolved**.
4. **Windows specifics.** The bootstrapper already displays the full GPL text
   (`license.rtf:4-8`) and now also names the corresponding-source URL
   directly in that same license screen (`license.rtf:7`) — kept in sync with
   the app license. The MSI and the portable zip carry the notices file +
   source pointer (items 1-2, resolved via `build-arch.bat`). The VC++ redist
   is downloaded, not embedded (`Bundle.wxs:63-101`); the CRT DLLs inside the
   *portable* zip (`scripts/build-arch.bat:310-313`) are confirmed permitted
   by the current MS redist documentation (§3.6, U8 resolved).
5. **macOS specifics.** `Jochona.app/Contents/Resources/` now carries
   `LICENSE.txt`, `THIRD-PARTY-NOTICES.txt`, `SOURCE-POINTER.txt`, and
   `VERSION.txt`, added before `codesign --deep` so they're part of the signed
   bundle (`scripts/generate-dmg.sh:74-80`). **Resolved.**
6. **AppImage specifics.** The distributed AppImage's corresponding source
   pointer (item 1) and the bundled license texts (item 2) are both now
   included at `usr/share/doc/jochona/` inside the AppDir before linuxdeploy
   packages it (`scripts/build-appimage.sh:62-68`). Bundle membership
   enumeration itself (U7's narrower residual claim) remains an open, lower-
   value follow-up — see §7.

## 5. ⚠️ Steam Link SDK — loud flag (now a hard gate, not just a warning)

- The zip we ship contains **no SDK files** — only our binary, our own
  metadata, and (once unblocked) the notices bundle
  (`scripts/build-steamlink-app.sh:65-71`). Good.
- **But the shipped binary statically links SDK-supplied code**: `-lSLVideo
  -lSLAudio` (`app/app.pro:433`) and `-lopus -larmasm -lNE10`
  (`app/app.pro:429`). The SDK is pulled from the public
  `ValveSoftware/steamlink-sdk` checkout (`build-steamlink.yml:27-33`, only when
  the gate below is open); **its license text is not in this repo, and the
  upstream repo itself carries none — confirmed UNKNOWN (§8).**
- **2026-08-30: this is now enforced, not just documented.**
  `scripts/build-steamlink-app.sh:14-25` refuses to build at all unless the
  caller explicitly sets `STEAMLINK_LICENSE_CONFIRMED=1`, with an error
  message pointing back at this section. `.github/workflows/build-steamlink.yml:27-49`
  gates the SDK checkout, the build step, and the artifact upload behind a
  `STEAMLINK_LICENSE_CONFIRMED` **repository variable** equal to `'true'` — a
  variable that does not exist in this repository today, so the CI job now
  runs a single "skipped" warning step and produces no Steam Link artifact at
  all. Nothing in this repository can accidentally publish a Steam Link build
  anymore; doing so requires a human to explicitly set that repository
  variable after obtaining a Valve license grant (or otherwise resolving the
  question) and to pass `STEAMLINK_LICENSE_CONFIRMED=1` to any local/manual
  build.
- Also still unresolved: whether the SDK's Qt is a static build — static LGPL
  Qt inside the binary would trigger object-file/source obligations for Qt
  itself on this artifact only. Determine this alongside the license
  question, before ever setting the confirmation variable.
- The v12 `steamlink.zip` dependency archive is now inspected: its Opus
  1.5.2 and Project Ne10 libraries are BSD-3-Clause and covered by the
  notices bundle. Only Valve's SLVideo/SLAudio and SDK Qt license grant
  remains unresolved.

## 6. UNKNOWN registry

| # | Unknown | Status |
|---|---|---|
| U1 | Steam Link SDK license (SLVideo/SLAudio) + SDK Qt static/dynamic — externally verified 2026-08-27 (§8): the SDK repo contains no license file at all | **Still gates the Steam Link artifact (§5) — now enforced by a hard build/CI gate, not fixable from this repo alone; needs a human license grant from Valve.** |
| U2 | Every license inside the moonlight-qt-deps v12 archives (Win/mac/SL): OpenSSL, FFmpeg (+config), SDL2, SDL_ttf (+embedded deps), opus, libplacebo, discord-rpc, NE10/armasm/opus statics | **Resolved across macOS, Windows x64/ARM64, and Steam Link archives on 2026-08-30 (§3.3), including SDL_ttf's pinned FreeType/HarfBuzz submodules.** |
| U3 | Qt 6.11.1 license text as actually deployed by windeployqt / macdeployqt / apt Qt6 | **Resolved 2026-08-30** via install-tooling verification (§3.2): LGPL-3.0-only, open-source edition, no commercial licensing configured anywhere in CI. |
| U4 | h264bitstream upstream provenance — resolved 2026-08-27 (§8): LGPL-2.1-or-later, compatible | **Fully resolved 2026-08-30**: the residual defect (stripped per-file headers) is fixed — upstream headers restored verbatim on all four vendored files (§9). |
| U5 | `ModeSeven.ttf` license — resolved 2026-08-30: font removed, replaced by the already-bundled OFL `Space Grotesk` | Resolved. |
| U6 | `discord.svg` usage rights — resolved 2026-08-30: asset removed, it was unreferenced dead weight | Resolved. |
| U7 | AppImage CI source-built deps' license texts + final linuxdeploy bundle membership | **License texts resolved 2026-08-30** (§3.5): SDL3, sdl2-compat, SDL_ttf, FFmpeg, dav1d, libplacebo, libva all license-identified against their pinned tags' own upstream files. **Still open:** the exact final bundle membership that linuxdeploy's dependency scan picks up at build time isn't enumerated in-repo — lower-value, deferred (see §7). |
| U8 | MS VC redist license coverage for loose CRT DLLs in the portable zip | **Resolved 2026-08-30** (§3.6): Microsoft's documented redistribution terms permit this exact usage (unmodified merge-module DLLs, licensed VS install). |
| U9 | Material icon SVGs attribution requirement | **Resolved 2026-08-30** (§3.7): Apache-2.0 attribution added, embedded in the binary and in the notices bundle. |

## 7. Actions (ordered, real gaps only)

Everything that can be fixed by writing code or documentation in this
repository is closed in this pass (§9). Remaining work:

1. **Before distributing any Steam Link build**, obtain a license grant for
   Valve's SLVideo/SLAudio and confirm the SDK Qt linkage terms. Only then
   set `STEAMLINK_LICENSE_CONFIRMED=true` in CI and pass
   `STEAMLINK_LICENSE_CONFIRMED=1` locally. Until then, the build and upload
   paths produce no artifact.
2. *(Lower value)* Record linuxdeploy's exact final AppImage bundle
   membership in a release-generated manifest.

## 8. Follow-up verification and closures (2026-08-27 by Main; 2026-08-30 closures)

Two UNKNOWNs resolved against upstream sources directly (outside the in-repo evidence rule) on 2026-08-27; several more closed in-repo and via further external verification on 2026-08-30 (§9 has the full list and evidence for the 2026-08-30 pass):

1. **U4 resolved — h264bitstream is LGPL-2.1-or-later.** Upstream `aizvorski/h264bitstream` header block (`h264_stream.h`): "either version 2.1 of the License, or (at your option) any later version." LGPL-2.1-or-later permits relicensing under LGPL-3.0, whose terms permit static combination with a GPL-3.0 program provided relink+source conditions ride along — satisfied here by the GPL source release. Compatible. **Residual defect, fixed 2026-08-30:** the vendored `h264bitstream/` copy in this repo deleted upstream's per-file license headers while keeping only the bare LICENSE text — restored verbatim (§9).
2. **U1 sharpened — Steam Link SDK has no license grant.** The public `ValveSoftware/steamlink-sdk` repository contains no LICENSE/COPYING file in its root; the README describes building and deploying applications but makes no license statement. The static-link question for `SLVideo`/`SLAudio` cannot be answered from documents. Practical mitigation already in place: the artifact ships no SDK files, and upstream Moonlight has distributed the same static build for years with Valve's knowledge (the SDK exists for third-party apps). **2026-08-30: turned into an enforced gate** (§5, §9) rather than relying on documentation alone.
3. U2/U3/U8/U9 are closed for directly shipped dependencies; release-generated AppImage membership remains the explicit residual in §7.
4. **U5 resolved, U6 resolved (2026-08-30).** The debug/stats overlay
   (`app/streaming/video/overlaymanager.cpp`) now loads the bundled,
   already-OFL-licensed `Space Grotesk` font instead of the
   unknown-provenance `ModeSeven.ttf`, which is deleted along with its
   `resources.qrc` entry. `res/discord.svg` was confirmed unreferenced by any
   QML `Image`/icon source and deleted along with its `resources.qrc` entry;
   this closes a dead-asset risk, not a Discord brand-terms review (there was
   never a live usage to review). Neither change touches the unrelated
   `discord-rpc` library (§3.3, license resolved 2026-08-30, still behind
   `CONFIG+=discord-rpc`).

## 9. 2026-08-30 packaging/notices closure pass — full evidence log

This section records the evidence behind the "RESOLVED 2026-08-30" claims.
The macOS dependencies were inspected from `libs/mac/`; the published v12
Windows x64, Windows ARM64, and Steam Link ZIP archives were downloaded and
inspected directly without checking their binaries into this repository.

**In-repo evidence (this repository's own files):**

- `libs/mac/include/openssl/opensslv.h` → `OPENSSL_VERSION_STR "3.6.3"`.
- `libs/mac/include/libavcodec/version_major.h` → `LIBAVCODEC_VERSION_MAJOR 63`;
  `libs/mac/include/libavutil/ffversion.h` → `FFMPEG_VERSION "d32b387"`.
- `libavcodec.63.dylib` (both arch slices) via `strings`: the compiled-in
  configure command (quoted in full in §3.3/§3.5) and the compiled-in
  license string `"libavcodec license: LGPL version 2.1 or later"`.
- `libs/mac/include/libplacebo/config.h:1-16` → LGPL-2.1-or-later header
  block, correcting the prior "publicly MIT" placeholder; `libplacebo.dylib`
  via `strings` → build string `v7.371.0`.
- `libs/mac/include/SDL2/SDL_copying.h`, `libs/mac/include/SDL3/SDL_copying.h`
  → full zlib text, Copyright (C) 1997-2026 Sam Lantinga.
- `libs/mac/include/SDL2/SDL_ttf.h:1-19` → full zlib text, Copyright (C)
  2001-2025 Sam Lantinga; `SDL_TTF_MAJOR_VERSION 2`, `SDL_TTF_MINOR_VERSION 25`.
- `libs/mac/include/SDL3/SDL_version.h` → `SDL_MAJOR_VERSION 3`,
  `SDL_MINOR_VERSION 4`, `SDL_MICRO_VERSION 14`.
- `libs/mac/include/opus.h:1-21` → partial (2-clause) BSD text.
- `.github/workflows/build-appimage.yml:42-147` → exact CI-pinned refs/commits
  and the exact FFmpeg configure line used for the from-source AppImage build.
- `.github/workflows/*.yml` grepped for `qt.*commercial|QT_LICENSE|qt-account`
  → no matches, supporting the Qt LGPL-3.0-only (open-source) conclusion.
- `git show b7adc70e:h264bitstream/h264_stream.h` (this repo's own history) →
  confirms the header stripping predates this fork; it happened when upstream
  moonlight-qt itself imported the minimal h264bitstream subset, not as a
  Jochona-introduced regression.

**External primary-source verification (methodology matching U1/U4/§8; the
license text is not vendored in this repository, so the upstream project's
own LICENSE/COPYING file was read directly on 2026-08-30):**

- `discordapp/discord-rpc` `LICENSE` → MIT, Copyright 2017 Discord, Inc.
- `videolan/dav1d` `COPYING` → BSD-2-Clause, Copyright © 2018-2025 VideoLAN
  and dav1d authors.
- `xiph/opus` `COPYING` → full BSD-3-Clause (IETF non-endorsement variant)
  text, confirming and completing the partial in-tree header.
- `intel/libva` `COPYING` → MIT-style (SGI/Precision-Insight) permissive text.
- `openssl/openssl` `LICENSE.txt` (master) → confirms the standard Apache-2.0
  text used for all 3.x releases, matching the 3.6.3 version identified above.
- `google/material-design-icons` `LICENSE` → Apache License 2.0.
- `aizvorski/h264bitstream` `h264_stream.h`, `bs.h`, `h264_nal.c` (master) →
  the exact per-file license header block restored onto the four vendored
  files in this repo.
- `learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files` →
  confirms unmodified VC++ Redistributable/merge-module DLL redistribution is
  permitted for licensed Visual Studio users, resolving U8.

**Files changed in this pass:**

- `app/deploy/notices/THIRD-PARTY-NOTICES.txt` (new) — the maintainable
  combined notices document described above; per-dependency sections with
  evidence citations, full texts for short permissive licenses, and shared
  sections for the long copyleft/Apache texts to avoid duplication.
- `app/deploy/notices/SOURCE-POINTER.txt` (new) — corresponding-source
  pointer shipped in every artifact.
- `app/res/LICENSE-MaterialIcons.txt` (new) — Apache-2.0 attribution for the
  Material icon SVGs, embedded into the binary via `app/resources.qrc:20`.
- `h264bitstream/h264_stream.h`, `h264_stream.c`, `h264_nal.c`, `bs.h` —
  upstream LGPL-2.1-or-later per-file headers restored verbatim, with a
  `Jochona:` note explaining the restoration and pointing at this file.
- `scripts/build-arch.bat` — copies `LICENSE`, the notices bundle, and
  `VERSION.txt` into the Windows deploy dir before the MSI harvest / portable
  zip.
- `scripts/generate-dmg.sh` — copies the same into
  `Jochona.app/Contents/Resources/` before `codesign --deep`.
- `scripts/build-appimage.sh` — copies the same into
  `usr/share/doc/jochona/` inside the AppDir before `linuxdeploy` packages it.
- `scripts/build-steamlink-app.sh` — hard license-confirmation gate added at
  the top of the script; notices copy added alongside the binary for if/when
  the gate is ever cleared.
- `wix/MoonlightSetup/license.rtf` — corresponding-source pointer paragraph
  added to the bootstrapper's license screen.
- `.github/workflows/build-steamlink.yml` — SDK checkout, build, and upload
  steps gated behind a `STEAMLINK_LICENSE_CONFIRMED` repository variable that
  does not exist today, so the job now no-ops with a warning instead of
  building/publishing an uncleared artifact.
- `app/resources.qrc` — one new `<file>` entry for
  `res/LICENSE-MaterialIcons.txt`.
