# Repository and release workflow

## Working safely

- Preserve existing local changes. Use a clean worktree from the requested base when the main checkout is dirty; do not reset it or update its submodules behind the user's back.
- Use `codex/` for new working branches unless the user requests another name. Respect an explicit request to commit directly to master.
- For Git ownership warnings, use a command-local `git -c safe.directory=<verified-worktree-path>` rather than changing global configuration.
- Inspect any more-specific `AGENTS.md` before changing a submodule or nested project. Dependency fixes belong in their owning repository.

## Release scope and authorization

Preparing a release means preparing and reviewing its contents. It does **not** by itself authorize creating/pushing a version tag, publishing a GitHub release, changing release visibility, or updating the appcast. Obtain explicit approval for publication and the exact release commit.

This repository derives versions from Git tags. A tag push is an external publishing action: the build workflows can upload release assets, and the Windows workflow can publish release notes and update the public auto-update feed. Do not run publishing scripts as a dry run.

## 1. Establish the release range

1. Confirm the target version, previous published version, and target branch. Tags use plain semantic versions such as `2.10.0`, without a `v` prefix.
2. Check status, fetch the remote branch and tags, and create or select a clean worktree. Record the candidate commit SHA so the audit has a fixed endpoint.
3. Inventory both the first-parent history and every commit in the range. Merge dates alone miss direct commits and cherry-picks; branch creation dates do not determine release membership.

   ```text
   git status --short
   git fetch origin master --tags
   gh release list --repo christofmuc/KnobKraft-orm --limit 10
   git rev-parse origin/master
   git log --first-parent --oneline <previous-tag>..<candidate-sha>
   git log --format=fuller <previous-tag>..<candidate-sha>
   git diff --stat <previous-tag>..<candidate-sha>
   git diff --submodule=log <previous-tag>..<candidate-sha>
   ```

4. Read the merged PRs, linked issues, original discussions and relevant diffs. Use `gh pr view <number> --json title,body,author,comments,reviews,closingIssuesReferences` and `gh issue view <number> --json title,body,author,comments,state`. Filter bot walkthroughs out of inventories, but verify their findings when relevant.
5. Audit the exact old/new submodule SHAs, not the dependency's current default branch. Confirm changed MidiKraft pins point to accessible, merged commits, not an unfinished feature branch. Inspect the dependency commit range for user-visible fixes.
6. Do not list an unmerged PR as shipped. Follow replacement/cherry-picked PRs back to the original contribution, but avoid counting integration merges as separate features.

## 2. Build the contribution and issue ledger

For every user-visible change, record its commit/PR, original issue or discussion, implementation scope, and the people who contributed:

- Code authors and co-authors, including authors of fork PRs later integrated by the maintainer.
- The original bug reporter, even when the maintainer created the issue by copying a discussion.
- People who supplied reproduction steps, captures, fixtures, protocol analysis, fixes, or hardware verification.
- Fixture authors and their redistribution/license provenance. A download URL alone is not permission to redistribute.

Verify exact GitHub handles from the source. Credit people inline next to the change, distinguishing reporting, coding and testing. Do not infer credit from a commit's committer alone, and do not present AI-generated review text as a human contribution. General thanks can acknowledge additional feedback without implying that an unrelated or still-open report was fixed.

Explicitly distinguish partial fixes from complete fixes. An open issue may have a shipped improvement; a closed issue is not proof that the current release contains the fix. Hardware reports, mock tests and manual-derived assumptions are different kinds of evidence. Check firmware notes and real-device evidence when a manual contradicts an owner's observations.

## 3. Complete the release notes

1. Edit `release_notes/<version>.md`, preserving and reconciling any existing draft. Read at least three prior notes first (for example `2.9.0.md`, `2.8.0.md`, and `2.7.0.md`).
2. Match the existing sections in this order: `## Features:`, `## Synths:`, `## Bug fixes:`. Omit genuinely empty sections. Use `*` bullets, `**\#123**` issue/PR references, bold synth names, and short, friendly descriptions. The maintainer's voice is direct and appreciative: "Thanks to @handle for the report and the fix!" Avoid a raw commit dump or generated marketing language.
3. Describe what changes for a user. Group related PRs; do not advertise internal cleanup as a new feature. Do not repeat synths or dependencies already released in the previous version as new additions.
4. Include model/OS limits, alpha/beta status, destructive audition behavior, and unverified hardware paths where relevant. Check the actual adaptation and setup help instead of copying a PR bot's support claims.
5. Put database backups, schema or fingerprint migration, reindexing, recording-file implications and older-host compatibility requirements prominently in the relevant entry. Do not call an explicit reindex operation an automatic migration.
6. Use absolute links for referenced documentation: release notes are rendered both on GitHub and in the separate appcast site, where repository-relative links break. Version-pinned links to the forthcoming tag are appropriate for the published notes; verify the target file exists in the candidate tree.
7. Reconcile every first-parent entry and meaningful dependency change against the notes. Each should be represented or deliberately excluded as internal/duplicate. Recheck all credited people against the contribution ledger.

## 4. Verify packaging and compatibility

- Every adaptation promised in the notes must exist in `adaptations/` and be included in `adaptations/CMakeLists.txt`'s `adaptation_files`. An entry in README alone does not ship it (see #542).
- Register applicable dedicated tests in `adaptation_files_test_shipped`; check support-module installation if new shared modules were added.
- Keep the synth listings in `README.md` and `docs/README.md` consistent with actual support and maturity. Preserve contributor credits.
- Check `The-Orm/gitversion.cmake`, `The-Orm/CMakeLists.txt`, and the workflows before assuming a hard-coded version bump is needed. `git describe` drives the application/package version and `ORM_VERSION`; `release_notes/<tag>.md` must exist before tagging.
- For adaptation API, database, or fingerprint changes, test legacy inputs and document the upgrade path. Never use the user's live database for migration experiments.

## 5. Validate the candidate

Use the dependencies in `requirements.txt` and the Python version used by current CI (currently 3.12). Run focused regressions while editing, then the full adaptation suite for release readiness:

```text
python -m pip install -r requirements.txt
cd adaptations
python -m pytest test_adaptations.py --adaptation "Sequential Pro 3.py" -q
python -m pytest --all . -q --no-header
```

The focused command is an example; select every affected adaptation and its dedicated tests. Report passed, failed and skipped tests honestly, and investigate unexpected skips. Mock MIDI verifies protocol sequences, not physical device timing.

For C++/database changes, initialize pinned submodules in the clean worktree (`git submodule update --init --recursive`), follow the platform workflow's CMake prerequisites, configure with `-DBUILD_PATCH_DATABASE_TESTS=ON`, build `patch_database_migration_test`, and run that executable. Do not assume `ctest` executes it: the current Windows workflow runs the binary directly. On Visual Studio builds it is normally under the selected configuration directory.

For documentation-only changes, check the history/credit ledger, Markdown rendering, links and `git diff --check`; do not claim that this replaces the release candidate's build/test gate.

Before publication, check CI for the **exact candidate SHA**, not an earlier green PR head. Inspect GitHub workflow state as well as YAML; disabled or missing jobs are not passing jobs. As audited for 2.10.0, Windows, macOS, Ubuntu 22 and Ubuntu 24 are active, while Arch Linux Docker and static Pages are manually disabled. Recheck this each release and report disabled coverage; do not silently enable it.

```text
gh api repos/christofmuc/KnobKraft-orm/actions/workflows
gh run list --repo christofmuc/KnobKraft-orm --commit <candidate-sha>
git diff --check
```

Record pending builds, unresolved reviews and hardware gaps. Inspect packaged artifacts to confirm new adaptations are included, and smoke-test startup and important import/send paths where hardware is available. If master changes during preparation, refresh the range and credits before declaring the notes complete.

## 6. Publish only after explicit approval

1. Have the maintainer approve the notes, release title, stable/prerelease status, and exact commit. Commit/push the preparation changes through the agreed workflow first. Ensure the chosen tag does not already exist locally or remotely; never move or force-push a published release tag.
2. With explicit publication approval, tag the approved clean commit and push only that tag. Do not use `git push --tags`, which might publish unrelated local tags.
3. Follow the tag-triggered platform builds through completion. Current publishing behavior:
   - Windows signs the installer for WinSparkle, uploads symbols, runs `write_appcast.py` and `make_github_release.py`, then uploads the installer.
   - `write_appcast.py` writes the public XML and rendered HTML notes in `christofmuc/appcasts`. It inserts an appcast item on each run; inspect existing state before retrying after a partial failure.
   - `make_github_release.py` creates a public, non-prerelease release (`draft=False`, `prerelease=False`) if none is found, and **does not update an existing release's notes**. Its lookup is not a dry-run or a complete reconciliation tool.
   - The other platform jobs also upload tag assets and may create/publish a release before Windows finishes. Do not assume publication is atomic or that a green appcast step means all assets exist. For a beta/draft release, reconcile the workflow flags first with maintainer approval.
4. Verify the actual release title, rendered body, tag SHA, visibility and all four expected assets (substitute the approved version):
   - `knobkraft_orm_setup_<version>.exe`
   - `KnobKraft_Orm-<version>-Darwin.dmg`
   - `KnobKraft_Orm-<version>-Linux.tar.gz` (Ubuntu 22 build)
   - `KnobKraft_Orm-<version>-Ubuntu24.tar.gz`
5. Check that the appcast references the correct installer and version, its signature is present, and its release-notes link renders correctly. Never expose signing keys or tokens in logs or notes.
6. If a publishing job fails, inspect the release and appcast before retrying. Do not delete/recreate assets, tags, releases or appcast entries without agreement on the recovery action. If an existing release body is stale, update it explicitly after approval rather than assuming the creation script overwrites it.
7. Notify relevant reporters/contributors of the available release when requested. Close only issues actually resolved; leave partial fixes and hardware-verification requests clearly tracked. Finish with the release link and a truthful summary of validation and remaining limitations.
