# Korg R3 protocol fixtures

`synthetic.syx` contains four program dumps (A1, A2, A8, B1) followed by one
edit-buffer dump. All parameter bytes are generated from scratch. They are
structural test data, not playable presets or hardware captures. The generator
and its output use this repository's dual licensing terms.

## Format evidence

The primary source is Korg's
[R3 MIDI Implementation](https://cdn.korg.com/us/support/download/files/b4ecd79e467cf57073b7650768e1a6dd.pdf),
printed pages 11-13:

- Section (9): program dump = `F0 42 3g 7D 4C pp PP`, packed data, `F7`.
- Section (8): edit-buffer dump = `F0 42 3g 7D 40`, packed data, `F7`.
- Note 1: 452 unpacked bytes become 517 MIDI-safe bytes: 64 full groups
  of seven bytes plus a final four-byte group, each with an MSB prefix.
- Note 5: prefix bit 0 holds the first source byte's MSB, bit 6 the seventh.
- Table 1: unpacked bytes 0-7 are the eight-character name.

Thus the complete messages have 525 and 523 bytes respectively. These are
specification-derived expectations; the downloaded editor files below do not
independently confirm the hardware's wire format.

## Public files inspected on 2026-09-04

The [Korg Forums R3 download index](http://www.korgforums.com/support/r3.htm)
provides the following examples:

| Archive | Entry | Entry bytes | Initial signature |
| --- | --- | ---: | --- |
| [hypervoc.zip](http://www.korgforums.com/support/r3/hypervoc.zip) | HyperVoc.r3p | 2,336 | `35 31 30 70` (`510p`) |
| [factory-default.zip](http://www.korgforums.com/support/r3/factory-default.zip) | R3 Default.r3l | 300,352 | `35 31 30 4c` (`510L`) |

Archive SHA-256 values:

```text
hypervoc.zip        ee2472980628d25afc3123715030d1ba6793e17d05837a06f54e791845ae684b
factory-default.zip 846bca29f7be6a07c2d0e2ab4f1d7ccd95fd10555d2804a081e4c1125a98838f
```

Neither file contains an `F0 42` sequence. They are editor-format files, not
raw Korg SysEx dumps. Each archive contains only the listed file, with no
license document. No explicit permission to redistribute these patches was
found on the index. They were inspected in temporary storage and are not
included here; no parameter data from them is used in these fixtures.

Korg also offers [Bonus Programs](https://www.korg.com/us/support/download/software/1/196/1423/)
and a [Power Bank](https://www.korg.com/us/support/download/software/1/196/1424/).
Their download pages carry Korg's software license, including redistribution
restrictions and an exception concerning derivative works based on data files.
They are not used as redistributable fixture sources here.

## Reproduction and coverage

From the repository root:

```text
python adaptations/testData/Korg_R3/generate_synthetic.py
cd adaptations
python -m pytest test_Korg_R3.py test_adaptations.py --adaptation Korg_R3.py
```

The standalone generator does not import the adaptation or its Korg helpers.
The checked-in bytes therefore remain stable if the adaptation's packing,
header, or size logic changes. The generic suite uses these fixtures for name,
rename, conversion, fingerprint, and mock MIDI checks. Dedicated tests cover
exact lengths, MSB order, the partial final group, and malformed messages.

The incomplete prefix in [issue #547](https://github.com/christofmuc/KnobKraft-orm/issues/547)
is tested as a rejected fragment, not padded and represented as a hardware
capture. The issue still needs a complete received dump to determine whether
its actual length or format differs from the specification, or whether the
problem lies in delivery to the application.
