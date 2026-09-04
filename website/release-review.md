# First website release review

Prepared locally for existing PR 519, `feature/website`. Nothing in this task authorizes a push, merge, deployment or GitHub message.

## Inputs and source reconciliation

- PR inspected remotely: open, head `a8736dd0e4f82487b43f35a5687eae9910c5cdd0`.
- Master fetched: `a7c5cb9ca9d7a747805c853e344217a42e655325`.
- Documentation snapshot: `819a7f1ef0b7534c3823fd61b61b5626242b9b79`; branding brief `7ac55be4`.
- Initiative audit baseline: `bf841664e9c31b2af3c83baadd58cdd7b03bc9b9`; it is not a release verification.
- Published release queried through GitHub: `2.9.0`, 19 April 2026; Windows EXE, macOS DMG, Linux and Ubuntu24 archives verified in the release asset list.
- Pages configuration read through GitHub: `build_type: workflow`, site URL `https://christofmuc.github.io/KnobKraft-orm/`. The legacy source field remains master `/docs`; the active build mode is Actions. The PR's branch-publishing workflow was therefore replaced with the official Pages artifact/deploy actions, keeping deploy gated to a push to master.

Core UI source between 2.9.0 and current master changes only the native MKS-50 registration/build references and a defensive empty-patch guard in `PatchView.cpp`. The audited setup, import, filter, list, save-copy and send-mode paths remain present. The pinned MidiKraft dependency changes: its delta concerns bank protocol handling, bank sending and MIDI messages; it does not establish new release or hardware validation. Read the actual `Synth::dataFileToSysex` fallback: without edit-buffer capability it can send to a stored program. This is why no universal non-writing audition promise remains.

The public FAQ reconciles community items CK-01, 03–08, 10, 14, 15 and 18 with the manual and source. It removes obsolete manual bank-save instructions and old macOS Python installation recipes, separates import counts from bank slots, distinguishes hidden flags from deletion, and does not expose internal community IDs/archives to readers. The advanced macro/capture/comparison chapters, curriculum IDs, unfinished videos and research archive are outside this release's publication selection.

The renamed programming guide is reconciled from master's fuller `adaptations/Adaptation Programming Guide.md` (not its stale docs copy), preserving custom program change, legacy loaders, message timings, all-bank-message extraction, host API requirements and explicit transfer-failure handshakes. The Arch Docker build instructions from master are retained. No application code or submodule pin is changed beyond the merge of master.

## Local review

Run `python scripts/build_website.py` and inspect the exact `public/` result. The homepage, three complete written lessons, seven manual chapters, glossary, curated FAQ, download page, refreshed compatibility table and existing developer guides are selected explicitly. The 1.0 introduction is identified as historical.

The visual evidence and executable browser checks are local under `build/website-qa/`; they are not public assets. See the task's final report for the completed checks and commit ID.

## Apply to the existing PR after review

The local branch `codex/website-release-prep` starts at the PR head and merges master while preserving the PR's website work. It is intended to fast-forward the existing PR branch after review, not to create another PR.

Before applying, fetch and recheck both remote heads. If `feature/website` is still the recorded head, fast-forward it to the prepared local branch and push **that existing branch** only after explicit authorization. If newer commits have appeared, integrate those first and rerun the website build. Do not force-push over newer work. The merge commit's ancestry carries upstream application changes, so do not cherry-pick it with an arbitrary mainline parent.

## Remaining release checks

1. Owner reviews the actual local pages, wording and identity; no broad design selection is needed.
2. Recheck latest release/asset availability immediately before publication. If 2.10.0 ships first, refresh the release column and downloads together.
3. Rehearse installation and representative MIDI workflows on actual hardware. This task validates the website and source-based descriptions, not audio or a universal hardware restore procedure.
4. Reconcile support status for packaged Teo-5 and Quasimidi Cyber-6, which are absent from the maintained master compatibility matrix. Do not invent a hardware-tested status from packaging alone.
5. Observe the first authorised master deployment and environment checks. The existing Pages Actions build mode was verified read-only and the workflow was corrected to match; its remote run is deliberately not triggered by this preparation task.

The site is reviewable with these explicit boundaries. No new video, transcript, device test result or release has been invented.
