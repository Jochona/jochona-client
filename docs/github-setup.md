# GitHub Setup — Jochona Client

Owner: `gogolB`, owner of the `Jochona` GitHub org (Free plan, created 2026-08-27). Everything marked CLI is reproducible by any org member with `gh` auth.

## Decisions baked in

- Repo home: `Jochona/jochona-client`, **private for now** (transferred from `gogolB/jochona-client` 2026-08-27). Flip to public before: (a) SignPath OSS eligibility (requires a public repo), (b) heavy macOS CI minutes (see Runners), (c) any distribution. GPL triggers source obligations on distribution, not on hosting.
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
gh repo delete gogolB/jochona-client --yes             # PENDING: needs `gh auth refresh -s delete_repo`
```

## Import the upstream fork (Milestone 0, run when ready)

```bash
git clone --bare https://github.com/moonlight-stream/moonlight-qt.git jochona-import.git
cd jochona-import.git
git push --mirror git@github.com:Jochona/jochona-client.git   # refuses to mix histories; see below
```

The mirror push will be rejected because `main` already holds the docs commit. Resolve by putting docs on top of upstream history:

```bash
git clone git@github.com:Jochona/jochona-client.git && cd jochona-client
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
./config.sh --url https://github.com/Jochona/jochona-client --unattended --ephemeral   # paste the ephemeral token from the dialog
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

### Linux distribution (Flathub is BLOCKED — verified 2026-08-27)

Flathub's [Requirements → Generative AI policy](https://docs.flathub.org/docs/for-app-authors/requirements) states: "Applications containing AI-generated or AI-assisted code, documentation, or any other content are not allowed," submission PRs "must not be generated, opened, or automated using AI tools or agents," and review text must not be LLM-generated. Exceptions exist only for "mature, well-maintained projects" — a repo this young will not qualify. Jochona's development process is AI-agent-heavy by choice; treat Flathub as **off the table by default**.

v1 plan: ship Flatpak from our own infrastructure —

1. CI builds a `.flatpakref` + `.flatpak` bundle per release, attached to GitHub Releases.
2. Users install with `flatpak install https://github.com/Jochona/jochona-client/releases/latest/download/jochona-client.flatpakref` (documented on the release page and website).
3. Runtime/deps from Flathub's `org.kde.Platform` runtimes (consumption side is unaffected by the policy).

Consequences to accept consciously: no Flathub search/browse discovery, no Flathub-run rebuilds, we host our own repo file update flow. If policy or project maturity changes the calculus later, a future submission also requires the app ID to be final (`app.jochona.client` once the domain is bought — Flathub verifies domain control; the GitHub-fallback prefix would have been `io.github.*`, not `com.github.*`) and a documented human-authored release of the submitted artifacts.

## Team access

Org teams (the repo is in the org now): create teams, then grant repo access per team.

```bash
gh api -X POST orgs/Jochona/teams -f name=dev -f privacy=closed
gh api -X PUT repos/Jochona/jochona-client/teams/dev -f permission=push
```

## Org migration — DONE 2026-08-27

1. Org `Jochona` created on the web (Free plan; org creation is web-only — no API).
2. Repo recreated under the org and pushed; `origin` repointed. `gh repo transfer` does not exist in gh CLI and the REST transfer requires a web accept, so create-push-delete was used. The stale `gogolB/jochona-client` copy still exists until deleted (`gh auth refresh -h github.com -s delete_repo`, then `gh repo delete gogolB/jochona-client --yes`).
3. **Free-plan limitation hit:** branch protection is unavailable on private org repos without GitHub Pro — the protection PUT returned 403 "Upgrade to GitHub Pro or make this repository public". Conventions (PRs, reviews) are voluntary until the public flip; re-apply protection at flip time.
4. When signing is set up, grant SignPath access against the `Jochona/jochona-client` path.

## CI (comes with the M0 import, not before)

The bare-mirror import brings upstream's Actions workflows with it — `build.yml` already runs a win/mac matrix on Qt 6.11.1 (`install-qt-action`, `scripts\build-arch.bat` + `generate-bundle.bat`, plus AppImage and Steam Link pipelines). M0's CI job is keeping those green through the rebrand, not authoring CI. The genuinely new release engineering: a workflow on `v*` tags running in the `release` environment that chains build → SignPath sign (Windows) → codesign + `xcrun notarytool` + staple (macOS) → `gh release create`. Upstream signs nothing; we sign everything.
