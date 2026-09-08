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

The release column was checked against tag `2.10.0` and `adaptations/CMakeLists.txt`. UB-Xa, CZ-101/1000, Nord Lead family, Mirage SoundProcess, Fourm and Trigon-6 are included in that release; their maturity and hardware-verification limits still apply. The MKS-50 Python replacement, SE-02 PRM import and Pro 3 bank/pacing updates are also included. Entries not packaged remain marked **Not in regular builds**. Teo-5 and Quasimidi Cyber-6 are packaged but absent from the maintained master matrix; their support status needs maintainer reconciliation before adding a new status claim here.

After editing the YAML:

```sh
python scripts/generate_supported_synths.py
python scripts/generate_supported_synths.py --check
```

Commit all three deterministic outputs: README table, supported-synths manual page, and `data/supported-synths.json`. Builds use `--check` and fail on stale output. No timestamp changes are generated on repeat runs. Update the release column and download page when the next release is published; do not label merged source as released merely because the build succeeds.

## Refresh the website after an app release

Complete this step after a new stable release and its assets are public, as part of the release procedure in `AGENTS.md`. Do not advertise a draft, prerelease or incomplete asset set as the latest stable release.

1. Query the published release and its assets with `gh release view <version> --repo christofmuc/KnobKraft-orm --json tagName,publishedAt,isDraft,isPrerelease,assets`. Confirm the intended version is also returned by `gh release view --repo christofmuc/KnobKraft-orm --json tagName`.
2. Update `docs/download.md`: checked version/date, platform table, all four asset URLs and release-notes link. Copy exact published asset URLs; filenames include the version, so changing only the URL tag or using `releases/latest/download/` with an old filename will break downloads. Keep the general `releases/latest` link as a fallback.
3. Update the checked-release wording in `index.html` and `docs/supported-synths.md`. Review `docs/data/supported-synths.yml` against the released tag and packaging manifest; update `release_checked`, `source_revision` and each row's availability together. Preserve alpha/beta status and hardware caveats; leave source-only additions labelled `After <version>`. Historical tutorial/video versions describe their original basis and must not be replaced mechanically.
4. Run `python scripts/generate_supported_synths.py`, then `python scripts/build_website.py` and `git diff --check`. Commit the sources and generated `README.md`, `docs/supported-synths.md` and `data/supported-synths.json` together. The local checker checks local links; the GitHub asset query and live checks cover external downloads.
5. Publish the website update through the authorized master workflow. Observe the **Website and Docs** workflow for the exact commit through successful deployment, then check the live homepage, download page, release-notes link and all four asset targets. Updating the app release alone does not refresh the website. Do not move the release tag or regenerate the appcast just to update website links.

## Tutorials and media

Stable pages under `docs/learn/` carry a `tutorial` ID. `tutorials.json` stores reviewed date, written-guide basis and optional published video metadata once. A video record requires a real YouTube ID, title, demonstrated version and synth. The hook renders the embedded video and attribution. An optional `transcript` names a reviewed, allowlisted Markdown file. Leave video/transcript values null when no published material exists; the page remains useful without empty video panels or promises.

The sole initial video is the existing `lPoFOVpTANM` introduction, explicitly labelled **1.0.0 / Prophet Rev2**. Its original screenshot is preserved and labelled historical. It is not a screenshot of release 2.9.0. Render the local screenshot as a link instead of embedding a remote player; the page must not contact YouTube before the reader follows that link.

## URLs and shared identity

The public domain is `https://knobkraft.com/`, with documentation under `/docs/`. Set the custom domain in GitHub Pages settings (Actions deployments do not use a `CNAME` file), keep HTTPS enforced once the certificate is ready, and keep the homepage canonical URL, MkDocs `site_url` and README links aligned. Navigation remains relative so local previews also work.

The existing MkDocs page URLs and homepage `#quick-start` / `#basic-concepts` anchors remain. The former Docsify `#/Adaptation Programming Guide`, testing-guide and README routes redirect through a committed script. The old spaced programming-guide path is a small reader-facing bridge. `docs/README.md` remains a repository bridge and is excluded from MkDocs because it shares the index URL.

Home links are resolved by the theme relative to each page's depth, so navigation also works below the GitHub Pages project prefix or a local preview prefix. There is no missing `home-link.js` dependency. The one remaining navigation script is committed and included in the allowlist.

`assets/tokens.css` is identity version 1: dark chassis, calmer reading panels, cyan actions, orange development accents, Inter prose, JetBrains Mono labels. The existing app mark is reused unmodified and credited to W07. The two variable fonts come from the Google Fonts `ofl/inter` and `ofl/jetbrainsmono` directories, with their OFL licenses alongside them. All styling/fonts are local; only opening the published video loads third-party media.

Use 16px body text on the homepage and compact 15px documentation text with 1.6 line height. Keep glow out of long-form reading, use normal product spelling in prose, and reserve the uppercase wordmark for branding. Frame real application imagery with a single quiet equipment-panel border. Do not imply DAW recall, universal edit-buffer safety or sound-similarity duplicate detection.

`assets/header.css` shares the header geometry between the landing page and the Material header override. Keep the main links in the same order and position across both; documentation search and the mobile contents control occupy a separate row below them.

## Validation and CI

`scripts/check_website.py` validates the built artifact, and is invoked by the build. Optional browser QA uses Playwright 1.62.1 with Chromium installed:

```sh
# Make Playwright resolvable in your local Node environment, then:
node website/browser-check.cjs
```

Set `SITE_URL` to a different preview root if needed, including a trailing slash. Screenshots/results go to ignored `build/website-qa/`. The checks cover desktop, 390px and 320px layouts, search/status/empty results, documentation search, mobile navigation, legacy routes, text enlargement and missing-data/no-JavaScript fallbacks.

The GitHub workflow builds on PRs with read-only repository permissions and uploads a Pages artifact. Its separate `actions/deploy-pages` job runs only on a **push to master**, after the validated artifact is available, with `pages: write` and `id-token: write` scoped to that job. This matches the repository's verified Actions-based Pages configuration. Local build scripts cannot invoke that deployment. A prepared PR can therefore be checked without publishing. See the [official deployment action](https://github.com/actions/deploy-pages) and [artifact action](https://github.com/actions/upload-pages-artifact).

See [release review](release-review.md) for the exact source reconciliation and remaining release checks.
