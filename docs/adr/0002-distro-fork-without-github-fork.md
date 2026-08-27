# Import Moonlight Qt history into our own repository instead of using a GitHub-native fork

GitHub-native forks cannot be private, cannot use Actions secrets, and cannot be detached from the fork network cleanly. We decided to copy moonlight-qt's full history into `lunaframe-client` via a bare-mirror push and add upstream as a named remote, syncing with regular merges.

## Consequences

- Full upstream history stays in-tree, which also serves GPL attribution and `git bisect` across upstream commits.
- Upstream contributions need a separate clone/remote push; `git push upstream` from this repo's branches only works if contributor permissions allow it, so plan on patch-based contributions.
