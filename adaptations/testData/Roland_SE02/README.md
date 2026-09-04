# SE-02 PRM import fixtures

Source: [MammaScan/se02-prm2syx](https://github.com/MammaScan/se02-prm2syx),
commit `6b3d5109d675a1cb01c9d2bf75bc2a3d5ddf0a18`, converter version 2.0.0.
The upstream MIT license is preserved in `LICENSE.prm2syx` and in the adaptation
so the attribution also ships with the standalone adaptation.

- `PATCH_65.PRM`: unmodified upstream `test/PATCH_65.PRM`. This is a sparse
  reference input, not a complete hardware backup.
- `PATCH_65_upstream.syx`: generated with the unmodified upstream converter,
  display slot 65 and its default template.
- `mapping_coverage.PRM`: synthetic input exercising all 56 upstream mapped
  parameters with a repeating set of byte boundary values (0, 15, 16, 127,
  128, 255, -1, -128). These are encoding tests, not playable patch values.
- `mapping_coverage_upstream.syx`: generated from that synthetic input by the
  same unmodified converter at display slot 65.

The golden SysEx files contain four messages at `06 40 00 00`, `06 40 00 40`,
`06 40 01 00`, and `06 40 01 40`. Tests compare the loader's output byte for
byte after adjusting only the address prefix to `05` and its checksums.
Each patch has 120 payload bytes encoded as 240 high/low nibbles, plus headers
and checksums, for a total of 296 bytes. The upstream template constant has
130 bytes, but its converter emits only the first 120; we retain those 120.

PRM conversion uses the upstream reverse-engineered mapping and template.
Missing parameters and unmapped bytes keep template defaults. The upstream
omits `COM_OCT`, `COM_TRNS`, `COM_PWM_DEPTH`, and `COM_PWM_RATE`; we preserve
that limitation. This does not establish lossless conversion of every backup.

The loader uses the adaptation's existing `05 bb` representation and filename
slot numbering (1–128 becomes 0–127); unnamed or unnumbered inputs default to
slot 0. Auditioning targets `05 00` through `convertToEditBuffer`. This agrees
with the edit-buffer address described by the independent reverse-engineering
project [Roland-SE-02-SYSEX-controller](https://github.com/PeZiK73/Roland-SE-02-SYSEX-controller#1-the-edit-buffer-address).
No changes to live MIDI requests or permanent storage are included.

Validation covers conversion parity, all mapped parameters, checksums,
filename handling, invalid input and audition retargeting. Hardware playback
has not been verified as part of this change.
