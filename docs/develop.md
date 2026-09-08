# Develop for KnobKraft

KnobKraft Orm combines a C++ application with Python synth adaptations. You can add an instrument without building the whole application, or work on the host when a new MIDI capability is needed.

## Add or improve a synth adaptation

1. Find the instrument's MIDI implementation and a few representative dumps you may use for testing.
2. Read the [Adaptation Programming Guide](programming-guide.md). Choose an existing integration with a similar protocol as a starting point.
3. Follow the [Adaptation Testing Guide](testing-guide.md) for fixtures, protocol simulation and hardware checks. Passing file tests alone does not establish safe live transfers.
4. Read [Contributing](contributing.md) before submitting your changes.

The programming guide tracks the current source. Check its host API requirements before copying a new adaptation into an older release.

## Work on the application

[Build from source](build.md) for Windows, macOS or Linux. Clone with submodules and use the toolchain requirements for the revision you build. [GitHub issues](https://github.com/christofmuc/KnobKraft-orm/issues) track bugs and feature work.

## Improve the documentation

The website and manual are Markdown and static HTML in the main repository. A useful correction names the affected build and explains the reader-visible behaviour. Include the page and the steps that led to the confusing result.

Read the [acknowledgements](acknowledgements.md) and [prior art](prior-art.md) for the people and projects behind KnobKraft.
