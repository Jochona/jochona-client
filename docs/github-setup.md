# GitHub Setup — Jochona Client

Owner: whoever holds the `gogolB` account for platform steps; everything marked CLI is reproducible by any teammate with `gh` auth.

## Decisions baked in

- Repo lives at `gogolB/jochona-client`, **private for now**. Flip to public before: (a) SignPath OSS eligibility (requires a public repo), (b) heavy macOS CI minutes (see Runners), (c) any distribution. GPL triggers source obligations on distribution, not on hosting.
- Org (`jochona`) comes later. GitHub does not allow org creation via API/token — it is a web-only step. Transferring the repo to the org later is one command and keeps everything.
- Moonlight Qt comes in as a bare-mirror history import into our own repo, not a GitHub-native fork (ADR-0002).
- Runners: GitHub-hosted only at first. Nothing to install.

## Done by CLI (already executed)

```bash
git init -b main && git add -A && git commit -m "Initial docs: proposal, glossary, ADRs"
gh repo create gogolB/jochona-client --private --source . --remote origin --push
gh api -X PUT repos/gogolB/jochona-client/environments/release   # gate for signing secrets
# branch protection: PR required before merge to main
```

## Import the upstream fork (Milestone 0, run when ready)

```bash
git clone --bare https://github.com/moonlight-stream/moonlight-qt.git jochona-import.git
cd jochona-import.git
git push --mirror git@github.com:gogolB/jochona-client.git   # refuses to mix histories; see below
```

The mirror push will be rejected because `main` already holds the docs commit. Resolve by putting docs on top of upstream history:

```bash
git clone git@github.com:gogolB/jochona-client.git && cd jochona-client
git checkout -b docs-backup main
# after the mirror is force-pushed to main by an admin, rebase docs back:
git rebase --onto main docs-backup~1 docs-backup   # or simply cherry-pick the docs commit
```

Alternative with no force-push: create the repo empty (delete and recreate without `--source`), push the mirror first, then cherry-pick the docs commit.

Then wire upstream sync:

```bash
git remote add upstream https://github.com/moonlight-stream/moonlight-qt.git
git remote set-url --push upstream DISABLED   # never push here by accident
gh variable set UPSTREAM_REPO --body https://github.com/moonlight-stream/moonlight-qt.git
```

Sync cadence: merge `upstream/master` into `main` at least weekly; `moonlight-common-c` is a submodule — keep it pinned and bump deliberately.

## Runners

No setup required initially — upstream's own workflows already pin their runner images (currently `windows-2025`, `macos-26` (Apple Silicon), `ubuntu-latest`) and build Windows x64 + ARM64, macOS, AppImage, and Steam Link on GitHub-hosted runners. Adopt them.

- Private repo on the Free plan: ~2,000 CI minutes/month, and macOS minutes consume at 10x. That is roughly 200 effective macOS minutes — enough for light per-PR builds, not for per-commit matrix builds. Verify current numbers at <https://docs.github.com/en/billing/understanding-billing/usage-limits-github-plans>.
- Flipping the repo public removes minute caps (macOS concurrency is still limited). Do this before CI-heavy M1 work if minute pressure appears.

Self-hosted runners: defer until actually needed (e.g., a Linux box or the Steam Deck for hardware-in-the-loop tests). Registration is: repo → Settings → Actions → Runners → New self-hosted runner, then on the machine:

```bash
mkdir actions-runner && cd actions-runner
curl -o runner.tar.gz -L https://github.com/actions/runner/releases/download/v2.327.1/actions-runner-linux-x64-2.327.1.tar.gz
tar xzf runner.tar.gz
./config.sh --url https://github.com/gogolB/jochona-client --unattended --ephemeral   # paste the ephemeral token from the dialog
sudo ./svc.sh install && sudo ./svc.sh start
```

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

### Flathub (Linux)

After the app identifier is final (`io.jochona.client` — pending name review, backlog item 1), open an app-request issue at <https://github.com/flathub/flathub/issues> with the manifest in a branch of your own repo. Flathub creates `flathub/io.jochona.Client`; builds run on their runners from your repo. No secrets needed on our side beyond a repo-scoped Flathub deploy key later.

## Team access

Personal repo: add collaborators by username (push or admin). After the org transfer, use org teams instead:

```bash
gh api -X PUT repos/gogolB/jochona-client/collaborators/USERNAME -f permission=push
```

## Org migration (later, web + CLI)

1. Web only: <https://github.com/organizations/plan> → create `jochona` org (Free plan). Check the name and confirm "Jochona" is clear for public use (backlog item 1) **before** creating the org.
2. Transfer from inside the local clone: `gh repo transfer jochona` — it updates `origin` automatically; accept the transfer in the org's notifications.
3. Environments and secrets travel with the repo; re-verify the `release` environment's protection rules and re-grant SignPath access for the new owner path.

## CI (comes with the M0 import, not before)

The bare-mirror import brings upstream's Actions workflows with it — `build.yml` already runs a win/mac matrix on Qt 6.11.1 (`install-qt-action`, `scripts\build-arch.bat` + `generate-bundle.bat`, plus AppImage and Steam Link pipelines). M0's CI job is keeping those green through the rebrand, not authoring CI. The genuinely new release engineering: a workflow on `v*` tags running in the `release` environment that chains build → SignPath sign (Windows) → codesign + `xcrun notarytool` + staple (macOS) → `gh release create`. Upstream signs nothing; we sign everything.
