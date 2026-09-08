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
- Maintain synth support in `docs/data/supported-synths.yml` and regenerate `README.md`, `docs/supported-synths.md` and `data/supported-synths.json` with `scripts/generate_supported_synths.py`. Keep actual support, release availability and contributor credits accurate; `docs/README.md` is a documentation bridge, not a second matrix.
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

Before publication, check CI for the **exact candidate SHA**, not an earlier green PR head. Inspect GitHub workflow state as well as YAML; disabled or missing jobs are not passing jobs. As checked on 4 September 2026, Windows, macOS, Ubuntu 22, Ubuntu 24 and Website and Docs (GitHub Pages) are active; Arch Linux Docker remains manually disabled. Pages was enabled with the owner's approval for the first website publication. Recheck this each release and report disabled coverage; do not silently enable it.

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
7. Refresh the public website after a stable release and all intended assets are verified. Follow **Refresh the website after an app release** in `website/README.md`: update the version/date, four exact asset URLs and release-notes link in `docs/download.md`; update the homepage and compatibility-page wording; reconcile `release_checked` and per-model availability in `docs/data/supported-synths.yml` against the tagged packaging manifest; regenerate the README/docs/JSON outputs; build and check the site. Versioned asset filenames require updating the full URLs, not just the tag segment. Do not present a draft, prerelease or source-only feature as released. Publish through the authorized master workflow, observe the exact commit's Pages deployment, and verify the live download targets. If website publication is not yet authorized, prepare these changes and report the pending website update. Never move the app release tag to update website links.
8. Notify relevant reporters/contributors of the available release when requested. Close only issues actually resolved; leave partial fixes and hardware-verification requests clearly tracked. Finish with the release link and a truthful summary of validation and remaining limitations.

## 7. Finish the release-ready issue lifecycle

The GitHub label is exactly `Ready for release`, on both issues and pull requests. It marks work in flight for a forthcoming release, not an archive of shipped changes.

1. Wait until the release's intended assets, release notes and appcast are published and verified. A successful build alone is not enough. During 2.10.0, an Ubuntu asset upload created an empty release before Windows ran; `make_github_release.py` then skipped creation and left the body empty. After approval, `gh release edit <version> --notes-file release_notes/<version>.md` repaired only the description. Verify the resulting text against the tagged notes; never rerun `write_appcast.py` just to repair the GitHub description.
2. When post-release housekeeping is authorized, enumerate labeled items in **all states**, including merged/closed PRs and closed issues, and include affected issues from the release ledger even if unlabeled. Use pagination; GitHub's REST issues endpoint includes PRs:

   ```text
   gh api --method GET repos/christofmuc/KnobKraft-orm/issues -f state=all -f "labels=Ready for release" -f per_page=100 --paginate
   ```

3. Match each item to a verified published version. Read release notes and issue discussions; check linked fix/merge commits against the release tag with `git merge-base --is-ancestor`. Check equivalent code for cherry-picks or squashes. Neither age, closure, nor a PR's merge alone establishes that a fix shipped. Distinguish a resolved subproblem from a broader request or later follow-up.
4. Re-read the current issue and comments before writing. Post one short release update with the release link, accurate scope, contributor/reporter thanks, and any migration or hardware caveats. Avoid duplicate comments on retries. Close fully resolved open issues as completed; leave partially resolved or genuinely unverified reports open and explain what remains. Do not close every issue merely mentioned in release notes.
5. Remove **only** `Ready for release` from verified shipped PRs and resolved shipped issues, preserving all other labels and PR state. Keep unresolved/ambiguous items labeled until their status is settled. Do not delete the label definition. Already closed historical items normally need only label removal, not redundant notification comments or reopening/reclosing.
6. For a historical sweep, keep an audit ledger outside the published release notes: item number/type, a verified containing release, evidence, action, and exceptions. Re-enumerate labeled items after cleanup, verify closures and comments, and report counts plus the items left in flight. Treat the sweep as a one-time operation; future releases should clear their own shipped items immediately after verification.
7. Workflow-documentation follow-ups after tagging must not move the published tag or silently change the approved release commit. Commit them separately and push only with authorization.
