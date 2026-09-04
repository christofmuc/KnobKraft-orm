# Suggested title

Release the KnobKraft website with Learn, manual and reviewed compatibility

# Suggested PR description

The website now gives a new user a complete path from checking an instrument and downloading KnobKraft to connecting MIDI, importing sounds and keeping a library. It retains the static landing page and MkDocs Material, refines the existing dark/cyan hardware identity, and reuses the application mark with locally bundled Inter and JetBrains Mono fonts.

The release includes three written lessons, seven core manual chapters and a glossary, a reconciled FAQ, current download assets, a 92-entry compatibility list with release availability, and the existing developer guides. Audition destinations, duplicate recognition and library/bank storage are described with device-specific limits. The original 1.0 video remains explicitly historical; no unpublished media is promised.

An explicit publication manifest keeps internal research and production material out of pages, copied assets and search. Generated compatibility output is deterministic and checked for drift. The build is independent of untracked output. Home and legacy documentation links resolve locally and under the project URL prefix. The Pages workflow matches the repository's Actions configuration, with build-only PR checks and deployment restricted to pushes to master.

Validation: strict complete-site build, local links/anchors/assets/search inventory, a negative publication-boundary check, and Chromium checks at 1440px, 390px and 320px covering navigation, filters, search, fallback states and text enlargement. The committed site inputs also build from an isolated archive. No application/hardware test or deployment is claimed.

Before release: review the rendered site, refresh release metadata if a new app release ships, reconcile the missing maintained status for packaged Teo-5/Cyber-6, and rehearse representative device workflows. Apply these commits to this existing PR branch; do not open a duplicate PR.
