# GitHub Setup — Jochona Client

Owner: `gogolB`, owner of the `Jochona` GitHub org (Free plan, created 2026-08-27). Everything marked CLI is reproducible by any org member with `gh` auth.

## Decisions baked in

- Repo home: `Jochona/jochona-client`, **PUBLIC since 2026-08-27** (FOSS posture confirmed by owner; org-private only during bootstrap). Consequences now live: unlimited Actions minutes (macOS concurrency still capped), branch protection available, SignPath OSS eligibility unblocked, GPL distribution obligations apply from the first published artifact.
- Org exists and is owned by the project — GitHub namespace `Jochona` verified collision-free (0 repos matched before creation; the `users/jochona` lookup resolves to the org itself).
- Moonlight Qt comes in as a bare-mirror history import into our own repo, not a GitHub-native fork (ADR-0002).
- Runners: GitHub-hosted only at first. Nothing to install.

## Done by CLI (already executed)

```bash
git init -b main && git add -A && git commit            # docs bootstrap under gogolB/
gh repo create gogolB/jochona-client --private --source . --remote origin --push
gh repo rename jochona-client --yes                    # Lunaframe -> Jochona rename
gh repo create Jochona/jochona-client --private        # org home (2026-08-27)
git remote set-url origin git@github.com:Jochona/jochona-client.git && git push -u origin main
gh api -X PUT repos/Jochona/jochona-client/environments/release
gh repo delete gogolB/jochona-client --yes             # DONE — owner deleted 2026-08-27, 404 verified
```

## Upstream import — DONE 2026-08-27

Executed as a history-preserving merge (equivalent to the bare-mirror plan, no force-push):

```bash
git remote add upstream https://github.com/moonlight-stream/moonlight-qt.git
git fetch --no-filter upstream master
git merge upstream/master --allow-unrelated-histories   # merge 8d38cff: docs history + full upstream history
git push origin main
git remote set-url --push upstream DISABLED             # accidental pushes are impossible
```

`main` now carries upstream `master` at tip `1da6ff43` beneath our commits. Sync cadence: merge `upstream/master` into `main` at least weekly; `moonlight-common-c` is a submodule — `git submodule update --init` after any bump and bump deliberately.

### GitHub Actions: enabled (repo-level, verified `enabled=true, allowed_actions=all, sha_pinning_required=false`)

Inherited workflows run as pushed: `build.yml` fires on every push (no branch filter) and PRs to `master` (upstream's original trigger; confirm this repo's default branch and trigger filters match `main` before relying on it), running the win x64/ARM64 + macOS matrix. The repo has been public since 2026-08-27 (line 7 above), so Actions minutes are unlimited (macOS concurrency still capped) — the Free-plan private-repo minute-burn concern below no longer applies. `build-steamlink`/`build-appimage` remain manual-dispatch. `i18n.yml`'s Linguist-extraction gate (see PRODUCT.md) also runs on PRs.

## Domain — DONE 2026-08-27

`jochona.com` purchased via Cloudflare; CNAME record set by owner. App id frozen as `com.jochona.client`. The website (GitHub Pages, custom domain `jochona.com` via the `Jochona/website` repo's `CNAME` file) is live, not future work; the Flatpak OSTree repo shares the same Pages infrastructure on a separate path. Cloudflare Email Routing for `hello@jochona.com` is worth enabling before any public contact page.

## Runners

No setup required initially — upstream's own workflows already pin their runner images (currently `windows-2025`, `macos-26` (Apple Silicon), `ubuntu-latest`) and build Windows x64 + ARM64, macOS, AppImage, and Steam Link on GitHub-hosted runners. Adopt them.

- The repo has been public since 2026-08-27 (Domain/Upstream import sections above), so the Free-plan private-repo CI-minute cap (~2,000 minutes/month, macOS at 10x) no longer applies — Actions minutes are unlimited on the public plan (macOS concurrency is still capped). This paragraph is retained only as historical context for the brief private-bootstrap window.

Self-hosted runners: defer until actually needed (e.g., a Linux box or the Steam Deck for hardware-in-the-loop tests). Registration is: repo → Settings → Actions → Runners → New self-hosted runner, then on the machine:

```bash
mkdir actions-runner && cd actions-runner
# Use the download URL shown in the dialog — it always carries the current runner version
# (latest as of 2026-08-27 is v2.337.0; the API is api.github.com/repos/actions/runner/releases/latest)
curl -o runner.tar.gz -L <url-from-dialog>
tar xzf runner.tar.gz
./config.sh --url https://github.com/Jochona/jochona-client --name steam-deck --labels steam-deck --unattended   # paste the registration token from the dialog
sudo ./svc.sh install && sudo ./svc.sh start
```

A persistent service runner must NOT use `--ephemeral`: an ephemeral runner unregisters itself after one job, which breaks the service. `--ephemeral` is only for autoscaling fleets without `svc.sh`.

Label hardware runners (e.g., `steam-deck`) and target them explicitly from workflows so normal CI never schedules onto them.

## Secrets and signing (platform steps — human required)

All release secrets go into the `release` GitHub environment, not repo secrets, so they are gated to tag-triggered deploys:

```bash
gh secret set SIGNPATH_API_TOKEN --env release --body "..."
gh secret set APPLE_KEY_ID      --env release --body "..."
gh secret set APPLE_ISSUER_ID   --env release --body "..."
gh secret set APPLE_API_KEY_P8_B64 --env release < Authenticator.p8.b64
gh secret set APPLE_CERT_P12_B64   --env release < JochonaDeveloperID.p12.b64
gh secret set APPLE_CERT_PASSWORD  --env release
gh secret set APPLE_TEAM_ID     --env release --body "..."
```

### Apple (you already have the Developer account)

1. App Store Connect → Users and Access → Integrations → App Store Connect API → create key with **Developer** role. Record **Issuer ID** (page header) and **Key ID**; download the `.p8` once. `base64 -i AuthKey_XXXX.p8 > Authenticator.p8.b64`.
2. Create a **Developer ID Application** certificate: on a dedicated Mac, Keychain Access → Certificate Assistant → save a CSR; Certificates, Identifiers & Profiles → Certificates → + → Developer ID → upload CSR; install, then export the identity as `.p12` with a strong password. `base64 -i cert.p12 > JochonaDeveloperID.p12.b64`. Delete the `.p12` and CSR after uploading the secret.
3. Nothing else on Apple's side is needed for notarization — `xcrun notarytool` consumes the API key from CI.

### SignPath (Windows code signing, free for OSS)

1. <https://app.signpath.io> → sign in with GitHub → create organization.
2. Requires: public repo + OSI-approved license (GPL-3.0 ✓). Do this after the visibility flip; the OSS agreement is signed once.
3. Create project `jochona-client`, upload the signing policy (release builds only), create a **CI user** and copy its API token. Select "GitHub" as the artifact source and grant SignPath read access to the repo when prompted.
4. CI uploads unsigned artifacts with `signpath-io/signpath-sign-files` (or the upload/download API); signed output comes back before packaging.

### Linux distribution (Flathub is BLOCKED — verified 2026-08-27)

Flathub's [Requirements → Generative AI policy](https://docs.flathub.org/docs/for-app-authors/requirements) states: "Applications containing AI-generated or AI-assisted code, documentation, or any other content are not allowed," submission PRs "must not be generated, opened, or automated using AI tools or agents," and review text must not be LLM-generated. Exceptions exist only for "mature, well-maintained projects" — a repo this young will not qualify. Jochona's development process is AI-agent-heavy by choice; treat Flathub as **off the table by default**.

v1 architecture — a `.flatpakref` only works when a live OSTree repo sits behind it, so CI hosts one:

1. CI runs `flatpak-builder --repo=repo …` + `flatpak build-update-repo repo`, pushes the repo tree to a `flatpak-repo` branch, and GitHub Pages serves it at `https://jochona.github.io/flatpak-repo/`.
2. Each release attaches `jochona-client.flatpakref` (pointing at the Pages repo URL) and a `.flatpak` bundle (manual `flatpak install --bundle` fallback).
3. Users: `flatpak install https://jochona.github.io/flatpak-repo/com.jochona.client.flatpakref` — update channel is the Pages repo; `.flatpakref` itself is static.
4. Repo signing: HTTPS delivery is the v1 integrity mechanism; optional detached GPG via `build-update-repo --gpg-sign` later.
5. Runtime/deps from Flathub's `org.kde.Platform` runtimes (consumption side is unaffected by the policy).

Consequences to accept consciously: no Flathub search/browse discovery, no Flathub-run rebuilds, we host our own repo file update flow. If policy or project maturity changes the calculus later, a future submission also requires the app ID `com.jochona.client` (reverse of the owned jochona.com domain — Flathub verifies domain control) and a documented human-authored release of the submitted artifacts.

## Team access — commands verified 2026-08-27 (team created and deleted as dry-run)

Org teams (the repo is in the org now): create teams, then grant repo access per team.

```bash
gh api -X POST orgs/Jochona/teams -f name=dev -f privacy=closed
gh api -X PUT repos/Jochona/jochona-client/teams/dev -f permission=push
```

## Org migration — DONE 2026-08-27

1. Org `Jochona` created on the web (Free plan; org creation is web-only — no API).
2. Repo recreated under the org and pushed; `origin` repointed. `gh repo transfer` does not exist in gh CLI and the REST transfer requires a web accept, so create-push-delete was used. The stale `gogolB/jochona-client` copy was deleted by the owner on 2026-08-27 (404 verified) — `Jochona/jochona-client` is the single home.
3. **Free-plan limitation (resolved by the public flip 2026-08-27):** branch protection was unavailable while private; now applied on `main` — PRs required for non-admins, `enforce_admins: false` while solo. Turn admin enforcement on when a second human joins.
4. SignPath (public-repo prerequisite now met): create org at app.signpath.io → project `jochona-client` → signing policy (release builds only) → CI user token → store as `release` environment secrets → grant SignPath repo read access. Apple Developer ID cert + ASC API key remain the last signing human-gate.

## CI (comes with the M0 import, not before)

The bare-mirror import brings upstream's Actions workflows with it — `build.yml` already runs a win/mac matrix on Qt 6.11.1 (`install-qt-action`, `scripts\build-arch.bat` + `generate-bundle.bat`, plus AppImage and Steam Link pipelines). M0's CI job is keeping those green through the rebrand, not authoring CI. The genuinely new release engineering: a workflow on `v*` tags running in the `release` environment that chains build → SignPath sign (Windows) → codesign + `xcrun notarytool` + staple (macOS) → `gh release create`. Upstream signs nothing; we sign everything.
