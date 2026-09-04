<img src="https://user-images.githubusercontent.com/5006524/277113738-73bf9b4f-f089-42b7-bbb2-aa4bf55c1528.png" align="right">

# KnobKraft Orm

[![release](https://img.shields.io/github/v/release/christofmuc/KnobKraft-orm?style=plastic)](https://github.com/christofmuc/KnobKraft-orm/releases)

A free, modern, cross-platform MIDI SysEx librarian for hardware synthesizers.

## Start Here

- Website and docs: <https://knobkraft.com/docs/>
- Install on Linux with Nix: <https://knobkraft.com/docs/download/#installing-on-linux-with-nix>
- Download latest release: <https://github.com/christofmuc/KnobKraft-orm/releases>
- Build from source: <https://knobkraft.com/docs/build/>
- Adaptation programming guide: <https://knobkraft.com/docs/programming-guide/>
- Adaptation testing guide: <https://knobkraft.com/docs/testing-guide/>
- Report issues / request synth support: <https://github.com/christofmuc/KnobKraft-orm/issues>

## Supported Synths

This table is generated from `docs/data/supported-synths.yml` by `scripts/generate_supported_synths.py`.

<!-- BEGIN:SUPPORTED_SYNTHS -->
| Manufacturer | Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- | --- |
| Access | Virus A, B, Classic, KB, Indigo | works | native | 2.10.0 |  |
| Access | Virus C | beta | native | 2.10.0 | Thanks to guavadude@gs |
| Akai | AX80 | beta | adaptation | 2.10.0 | Thanks to O.S.R.C. on YT for the nudge |
| Alesis | Andromeda A6 | works | adaptation | 2.10.0 | Thanks to @markusschloesser |
| Behringer | BCR2000 | in progress | native | 2.10.0 |  |
| Behringer | Deepmind 12 | works | adaptation | 2.10.0 |  |
| Behringer | Pro-800 | alpha | adaptation | 2.10.0 | Thanks to @Andy2No |
| Behringer | RD-8 | in progress | adaptation | Not in regular builds |  |
| Behringer | RD-9 | in progress | adaptation | Not in regular builds |  |
| Behringer | UB-Xa | alpha | adaptation | 2.10.0 | Contributed by @Casuallynoted; hardware validation needed |
| Behringer | Wave | works | adaptation | 2.10.0 | Thanks to @willxy! |
| Black Corporation | Kijimi | beta | adaptation | 2.10.0 | Thanks to @ffont and @markusschlosser |
| Casio | CZ-101/CZ-1000 | alpha | adaptation | 2.10.0 | Contributed by @Casuallynoted; hardware validation needed |
| Clavia | Nord Lead / Nord Lead 2 / Nord Lead 2X | alpha | adaptation | 2.10.0 | Four documented live-request banks (availability depends on model and installed memory card); ten-bank 2X file import/export; official fixtures and mock MIDI tested, hardware verification pending; no Nord Lead 3+ |
| DSI | Evolver | beta | adaptation | 2.10.0 |  |
| DSI | Mopho | works | adaptation | 2.10.0 |  |
| DSI | Mopho X4 | works | adaptation | 2.10.0 |  |
| DSI | Tetra | works | adaptation | 2.10.0 |  |
| DSI | Pro 2 | works | adaptation | 2.10.0 |  |
| DSI | Prophet 8 | works | adaptation | 2.10.0 |  |
| DSI | Tempest | alpha | adaptation | 2.10.0 |  |
| DSI/Sequential | OB-6 | works | native | 2.10.0 |  |
| DSI/Sequential | Prophet Rev2 | works | native | 2.10.0 |  |
| DSI/Sequential | Prophet 12 | works | adaptation | 2.10.0 | Thanks to @Andy2No |
| Elektron | Analog Rytm | beta | adaptation | 2.10.0 | Thanks to @RadekPilich for the request! |
| Elektron | Digitone | alpha | adaptation | 2.10.0 | This needs more work, owners please provide feedback so we can complete it. |
| E-mu | Morpheus | works | adaptation | 2.10.0 | Thanks to Kid Who for testing! |
| Ensoniq | ESQ-1/SQ-80 | works | adaptation | 2.10.0 | Contributed by @Mostelin! |
| Ensoniq | Mirage (SoundProcess) | alpha | adaptation | 2.10.0 | Contributed by @Casuallynoted; alternate SysEx header needs hardware validation |
| Ensoniq | VFX/VFX-SD | works | adaptation | 2.10.0 | Thanks to @dancingdog for testing! |
| Groove Synthesis | 3rd Wave | works | adaptation | 2.10.0 |  |
| John Bowen | Solaris | beta | adaptation | 2.10.0 | Contributed by @conversy! |
| Kawai | K1/K1m/K1r | beta | adaptation | 2.10.0 |  |
| Kawai | K3/K3m | works | native | 2.10.0 |  |
| Kawai | K4 | alpha | adaptation | 2.10.0 |  |
| Kawai | K5000 | beta | adaptation | 2.10.0 | Heroic effort by @markusschlosser! Most complex! |
| Korg | 03R/W | works | adaptation | 2.10.0 | Thanks to Philippe! |
| Korg | DW-6000 | works | adaptation | 2.10.0 |  |
| Korg | DW-8000/EX-8000 | works | adaptation | 2.10.0 |  |
| Korg | M1 | works | adaptation | 2.10.0 | Thanks to Jentusalentu at YT for giving the nudge |
| Korg | microKORG S | works | adaptation | 2.10.0 | Thanks to @ilantz! |
| Korg | Minilogue XD | works | adaptation | 2.10.0 | Thanks to @andy2no |
| Korg | MS2000/microKORG | works | adaptation | 2.10.0 | Thanks to @windo |
| Korg | R3 | alpha | adaptation | 2.10.0 | 100% AI generated |
| Line 6 | POD Series | works | adaptation | 2.10.0 | Thanks to @milnak! |
| Moog | Voyager | works | adaptation | 2.10.0 | Thanks to @troach242 for the nudge and test! |
| Novation | AStation/KStation | beta | adaptation | 2.10.0 | Thanks to @thechildofroth |
| Novation | Bass Station II | works | adaptation | 2.10.0 | Thanks to @cockroach! |
| Novation | Summit/Peak | alpha | adaptation | 2.10.0 |  |
| Novation | UltraNova | works | adaptation | 2.10.0 | Thanks to @nezetic |
| Oberheim | Matrix 6/6R | works | adaptation | 2.10.0 | Thanks to @tsantilis |
| Oberheim | Matrix 1000 | works | native | 2.10.0 |  |
| Oberheim | OB-X (Encore) | alpha | adaptation | 2.10.0 |  |
| Oberheim | OB-Xa (Encore) | alpha | adaptation | 2.10.0 |  |
| Oberheim | OB-8 | beta | adaptation | 2.10.0 |  |
| Oberheim | OB-X8 | beta | adaptation | 2.10.0 | help needed! |
| Pioneer | Toraiz AS-1 | works | adaptation | 2.10.0 | Thanks to @zzort! |
| Roland | JX-8P | alpha | adaptation | 2.10.0 |  |
| Roland | Juno-DS | works | adaptation | 2.10.0 | contributed by @mslinn! Thank you! |
| Roland | D-50 | works | adaptation | 2.10.0 | Shout out to @summersetter for testing! |
| Roland | JD-Xi | alpha | adaptation | 2.10.0 | 100% AI generated |
| Roland | JV-80/880/90/1000 | beta | adaptation | 2.10.0 |  |
| Roland | JV-1080/2080 | beta | adaptation | 2.10.0 |  |
| Roland | MKS-50 | alpha | adaptation | 2.10.0 |  |
| Roland | MKS-70 (Vecoven) | beta | adaptation | 2.10.0 | Thanks to @markusschloesser! |
| Roland | MKS-80 | works | native | 2.10.0 |  |
| Roland | SE-02 | beta | adaptation | 2.10.0 | DT1 dumps and USB backup PRM import (MammaScan converter mapping). Thanks to @MammaScan! |
| Roland | U-20/U-220 | alpha | adaptation | 2.10.0 | 100% AI generated. Thanks to @Casuallynoted for testing! |
| Roland | V-Drums TD-07 | alpha | adaptation | 2.10.0 |  |
| Roland | XV-3080/5080/5050 | works | adaptation | 2.10.0 |  |
| Sequential | Fourm | beta | adaptation | 2.10.0 | Real dump tested; hardware verification pending |
| Sequential | Pro 3 | works | adaptation | 2.10.0 | User/factory bank metadata and transfer pacing thanks to @RadekPilich |
| Sequential | Prophet-5 Rev 4 | works | adaptation | 2.10.0 |  |
| Sequential | Prophet-6 | beta | adaptation | 2.10.0 |  |
| Sequential | Prophet X | works | adaptation | 2.10.0 |  |
| Sequential | Take 5 | beta | adaptation | 2.10.0 |  |
| Sequential | Trigon-6 | works | adaptation | 2.10.0 |  |
| Studiologic | Sledge | beta | adaptation | 2.10.0 |  |
| Waldorf | Blofeld | beta | adaptation | 2.10.0 |  |
| Waldorf | M | works | adaptation | 2.10.0 | Thanks to @RadekPilich for testing! |
| Waldorf | MicroWave 1 | beta | adaptation | 2.10.0 | Thanks to Gerome S! |
| Waldorf | Kyra | alpha | adaptation | 2.10.0 | Thanks to Edisyn! |
| Waldorf | Pulse | works | adaptation | 2.10.0 | Thanks to @markusschlosser and chatGPT! |
| Yamaha | DX7 | beta | adaptation | 2.10.0 |  |
| Yamaha | DX7II | works | adaptation | 2.10.0 | Thanks to @AgtSlick for testing and the fixes! |
| Yamaha | FS1R | alpha | adaptation | 2.10.0 | Thanks to @markusschlosser for testing! |
| Yamaha | reface DX | works | adaptation | 2.10.0 |  |
| Yamaha | reface CP | beta | adaptation | 2.10.0 | Thanks to @milnak! |
| Yamaha | TX7 | works | adaptation | 2.10.0 | Thanks to Gerome S! |
| Yamaha | TX81Z | works | adaptation | 2.10.0 | Contributed by @summersetter! |
| Yamaha | Yamaha YC61/YC73/YC88 | works | adaptation | 2.10.0 | Thanks to @milnak! |
| Zoom | MS Series (50G/60B/70CDR) | works | adaptation | 2.10.0 | Thanks to @nezetic |
<!-- END:SUPPORTED_SYNTHS -->

If a synth is missing, or a status needs updating, open an issue (and ideally include test data).
