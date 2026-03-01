# KnobKraft Orm User Manual

KnobKraft Orm is a free, modern, cross-platform MIDI SysEx librarian for hardware synthesizers.

## Quick Start

1. Download the latest installer or DMG from GitHub Releases: <https://github.com/christofmuc/KnobKraft-orm/releases>
2. Connect your synth and MIDI interface.
3. Open KnobKraft Orm and go to setup to enable your synth.
4. Import banks/files, browse patches, and audition directly from the librarian.

For adaptation developers, start with the [Adaptation Programming Guide](./programming-guide.md) and validate work with the [Adaptation Testing Guide](./testing-guide.md).

## Basic Concepts

- `Database-first workflow`: patches are stored and searchable in a local database.
- `Auditioning`: selecting a patch sends it to the synth edit buffer (when supported) so you can hear it without overwriting program memory.
- `Adaptations`: synth support is implemented as Python scripts, allowing the community to add hardware quickly.

## Documentation Map

- [Supported Synths](./supported-synths.md)
- [Download and Install](./download.md)
- [Build from Source](./build.md)
- [Contributing](./contributing.md)
- [Acknowledgements](./acknowledgements.md)
- [Prior Art](./prior-art.md)

## In the Press

A practical intro from @mslinn:
<https://mslinn.com/av_studio/720-knobkraft.html>
