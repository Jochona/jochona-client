# Installing Jochona Client

Jochona Client has no stable releases yet — there is no GitHub Releases page,
no Flathub listing, and no winget/Homebrew package. Every build comes from
GitHub Actions artifacts produced by [`.github/workflows/build.yml`](../.github/workflows/build.yml),
which runs on pull requests and on manual dispatch (see
[`docs/github-setup.md`](github-setup.md)).

## Fetching a build with `gh`

1. Trigger a build (or use an existing pull request's run instead of
   triggering a new one):

   ```bash
   gh workflow run build.yml --repo Jochona/jochona-client --ref main
   ```

2. Find the run you just started:

   ```bash
   gh run list --repo Jochona/jochona-client --workflow=build.yml --limit 1
   ```

3. Download its artifacts once the run finishes:

   ```bash
   gh run download <run-id> --repo Jochona/jochona-client -D ./jochona-client-build
   ```

   This pulls every artifact for the run. Pass `--name <artifact-name>` to
   grab only one (see the per-platform names below). `gh run download` waits
   for nothing — re-run it after the run shows `completed` in
   `gh run view <run-id> --repo Jochona/jochona-client`.

Artifacts are named `Jochona-<os>-<arch>-<short-sha>` (or
`Jochona-LinuxAppImage-<short-sha>` for Linux); GitHub zips each artifact on
download.

## Windows

Artifact: `Jochona-Windows-x64-<ver>` or `Jochona-Windows-arm64-<ver>`.

The CI artifact is the raw deployment folder (`Jochona.exe`, Qt DLLs, FFmpeg
DLLs, and the Visual C++ redistributable DLLs already copied in) — there is
no installer in the artifact, only the portable form. Unzip it anywhere and
run `Jochona.exe` directly; nothing else to install.

## macOS

Artifact: `Jochona-macOS-<ver>`, containing a `.dmg`.

This fork's CI does not codesign or notarize (no Apple signing secrets are
configured — see `docs/github-setup.md`), so the app inside the DMG is
unsigned. Gatekeeper will refuse to open it with a plain double-click;
right-click the app → **Open** → **Open** on the warning dialog, or clear the
quarantine flag yourself:

```bash
xattr -cr /Applications/Jochona.app
```

## Linux (AppImage)

Artifact: `Jochona-LinuxAppImage-<ver>`, containing
`Jochona-<ver>-x86_64.AppImage`.

The AppImage bundles its own Qt, SDL3, FFmpeg, and VA-API/VDPAU libraries
(built from source in CI), so no distro packages are required to run it.

```bash
chmod +x Jochona-<ver>-x86_64.AppImage
./Jochona-<ver>-x86_64.AppImage
```

### Bazzite

Bazzite is an immutable Fedora variant with no writable `/usr`, which makes
AppImage the right fit — nothing to `rpm-ostree install`. Drop the file
anywhere under your home directory (e.g. `~/Applications/`), `chmod +x` it,
and run it; no layering, no reboot. To launch it like an installed app, add
it as a non-Steam shortcut from Game Mode or desktop mode, or create a
`.desktop` file pointing at the AppImage path under
`~/.local/share/applications/`.

### Flatpak

Flatpak packaging is **planned, not available**. `docs/github-setup.md`
describes a v1 design (CI-built OSTree repo served over GitHub Pages, since
Flathub itself currently blocks AI-assisted submissions), but no Flatpak
build workflow exists in this repository yet. Use the AppImage artifact
instead.

## Pairing with a Host

1. Make sure Jochona Host is running on the Windows machine you want to
   stream from.
2. Open Jochona Client. If the Host is on the same LAN, it should appear
   automatically via mDNS discovery; if it doesn't (different subnet, mDNS
   blocked by a firewall/VLAN), add it manually by IP address.
3. Select the Host. The Client displays a pairing PIN.
4. On the Host machine, open `https://localhost:47990` in a browser, go to
   its pairing page, and enter the PIN shown on the Client. The PIN expires
   quickly (around 60 seconds), so do this immediately.
5. Once paired, the Host shows as trusted in the Client and you can launch a
   stream.

## Wake-on-LAN

The Client can store the Host's MAC address (captured automatically once
you've paired over HTTPS) and send it a magic packet to wake it from sleep
or a powered-off state before connecting.

This is **direct, same-LAN wake only** in the current design (see
`docs/adr/0004-direct-wake-only-v1.md`): the Client and the Host must share a
layer-2 broadcast domain. It will not wake a Host across a VPN/overlay
network (Tailscale, WireGuard) or across routed subnets — there is no relay
or remote-wake path yet. The optional Jochona Beacon daemon (Linux-only) can
take over wake duty as a durable always-on helper on the Host's LAN, but it
is not required for a basic same-network Host + Client setup.
