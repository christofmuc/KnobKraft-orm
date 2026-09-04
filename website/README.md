# Website maintenance

The site stays static HTML at `/` plus MkDocs Material at `/docs/`. Build it with Python 3.12:

```sh
python -m venv .venv
# Activate .venv using your platform's command, then:
python -m pip install -r website/requirements.txt
python scripts/build_website.py
python -m http.server 8519 --bind 127.0.0.1 --directory public
```

Open <http://127.0.0.1:8519/>. The build never publishes or contacts GitHub. It replaces only the generated `public/` directory, checks generated compatibility data, builds MkDocs in strict mode, then checks output inventory, local links, anchors, CSS assets and search entries. Serve `public/`, never the repository or research tree.

## Public content boundary

`publication.json` is the explicit publication contract. Its page and asset lists select source files; navigation is not an access boundary. `hooks.py` removes unselected repository files before MkDocs renders or copies them and adds the shared font, mark and token assets. The output checker rejects unexpected pages/files and search entries. Material's installed theme assets are allowed separately.

The maintained reader manual is **`docs/manual/*.md`**, with its contents page at `docs/index.md`. Edit those canonical chapters. The build reads them directly; do not create a second staged manual for hand editing. The initial edition imports seven core chapters and the glossary from documentation snapshot `819a7f1ef0b7534c3823fd61b61b5626242b9b79`, then reconciles the prose and links for public use. Later merges of the documentation initiative must preserve these editorial changes rather than overwrite them with the earlier drafts.

Production scripts, video plans, raw API responses, task records and internal provenance are not selected and do not belong in the public output. They need not be merged into this branch. To add a page, review its content, list it in `publication.json`, add navigation or a meaningful incoming link, and run the full build.

## Synth compatibility

`docs/data/supported-synths.yml` is authoritative after this reconciliation. Status labels were preserved from current master README `a7c5cb9ca9d7a747805c853e344217a42e655325`, not copied from the stale legacy `docs/README.md`. The 92 rows are named synth/family entries, not a count of verified devices or guaranteed workflows. Status is not inferred from file existence or a passing unit test.

The release column was checked against tag `2.9.0` and `adaptations/CMakeLists.txt`. UB-Xa, CZ-101/1000, Nord Lead family, Mirage SoundProcess, Fourm and the newly packaged Trigon-6 are marked **After 2.9.0**. In-progress entries are marked outside regular builds. Newer MKS-50, SE-02 PRM and Pro 3 functionality is called out above the table. Teo-5 and Quasimidi Cyber-6 are packaged but absent from the maintained master matrix; their support status needs maintainer reconciliation before adding a new status claim here.

After editing the YAML:

```sh
python scripts/generate_supported_synths.py
python scripts/generate_supported_synths.py --check
```

Commit all three deterministic outputs: README table, supported-synths manual page, and `data/supported-synths.json`. Builds use `--check` and fail on stale output. No timestamp changes are generated on repeat runs. Update the release column and download page when the next release is published; do not label merged source as released merely because the build succeeds.

## Tutorials and media

Stable pages under `docs/learn/` carry a `tutorial` ID. `tutorials.json` stores reviewed date, written-guide basis and optional published video metadata once. A video record requires a real YouTube ID, title, demonstrated version and synth. The hook renders the embedded video and attribution. An optional `transcript` names a reviewed, allowlisted Markdown file. Leave video/transcript values null when no published material exists; the page remains useful without empty video panels or promises.

The sole initial video is the existing `lPoFOVpTANM` introduction, explicitly labelled **1.0.0 / Prophet Rev2**. Its original screenshot is preserved and labelled historical. It is not a screenshot of release 2.9.0.

## URLs and shared identity

The existing MkDocs page URLs and homepage `#quick-start` / `#basic-concepts` anchors remain. The former Docsify `#/Adaptation Programming Guide`, testing-guide and README routes redirect through a committed script. The old spaced programming-guide path is a small reader-facing bridge. `docs/README.md` remains a repository bridge and is excluded from MkDocs because it shares the index URL.

Home links are resolved by the theme relative to each page's depth, so navigation also works below the GitHub Pages project prefix or a local preview prefix. There is no missing `home-link.js` dependency. The one remaining navigation script is committed and included in the allowlist.

`assets/tokens.css` is identity version 1: dark chassis, calmer reading panels, cyan actions, orange development accents, Inter prose, JetBrains Mono labels. The existing app mark is reused unmodified and credited to W07. The two variable fonts come from the Google Fonts `ofl/inter` and `ofl/jetbrainsmono` directories, with their OFL licenses alongside them. All styling/fonts are local; only opening the published video loads third-party media.

Use at least 16px body text, keep glow out of long-form reading, use normal product spelling in prose, and reserve the uppercase wordmark for branding. Frame real application imagery with a single quiet equipment-panel border. Do not imply DAW recall, universal edit-buffer safety or sound-similarity duplicate detection.

## Validation and CI

`scripts/check_website.py` validates the built artifact, and is invoked by the build. Optional browser QA uses Playwright 1.62.1 with Chromium installed:

```sh
# Make Playwright resolvable in your local Node environment, then:
node website/browser-check.cjs
```

Set `SITE_URL` to a different preview root if needed, including a trailing slash. Screenshots/results go to ignored `build/website-qa/`. The checks cover desktop, 390px and 320px layouts, search/status/empty results, documentation search, mobile navigation, legacy routes, text enlargement and missing-data/no-JavaScript fallbacks.

The GitHub workflow builds on PRs with read-only repository permissions and uploads a Pages artifact. Its separate `actions/deploy-pages` job runs only on a **push to master**, after the validated artifact is available, with `pages: write` and `id-token: write` scoped to that job. This matches the repository's verified Actions-based Pages configuration. Local build scripts cannot invoke that deployment. A prepared PR can therefore be checked without publishing. See the [official deployment action](https://github.com/actions/deploy-pages) and [artifact action](https://github.com/actions/upload-pages-artifact).

See [release review](release-review.md) for the exact source reconciliation and remaining release checks.
