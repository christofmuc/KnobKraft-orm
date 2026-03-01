<img src="https://user-images.githubusercontent.com/5006524/277113738-73bf9b4f-f089-42b7-bbb2-aa4bf55c1528.png" align="right">

# KnobKraft Orm

[![release](https://img.shields.io/github/v/release/christofmuc/KnobKraft-orm?style=plastic)](https://github.com/christofmuc/KnobKraft-orm/releases)

A free, modern, cross-platform MIDI SysEx librarian for hardware synthesizers.

## Start Here

- Website and docs: <https://christofmuc.github.io/KnobKraft-orm/docs/>
- Download latest release: <https://github.com/christofmuc/KnobKraft-orm/releases>
- Build from source: <https://christofmuc.github.io/KnobKraft-orm/docs/build/>
- Adaptation programming guide: <https://christofmuc.github.io/KnobKraft-orm/docs/programming-guide/>
- Adaptation testing guide: <https://christofmuc.github.io/KnobKraft-orm/docs/testing-guide/>
- Report issues / request synth support: <https://github.com/christofmuc/KnobKraft-orm/issues>

## Supported Synths

This table is generated from `docs/data/supported-synths.yml` by `scripts/generate_supported_synths.py`.

<!-- BEGIN:SUPPORTED_SYNTHS -->
| Manufacturer | Synth | Status | Type | Kudos |
| --- | --- | --- | --- | --- |
| Access | Virus A, B, Classic, KB, Indigo | works | native |  |
| Access | Virus C | beta | native | Thanks to guavadude@gs |
| Akai | AX80 | beta | adaptation | Thanks to O.S.R.C. on YT for the nudge |
| Alesis | Andromeda A6 | works | adaptation | Thanks to @markusschloesser |
| Behringer | BCR2000 | in progress | native |  |
| Behringer | Deepmind 12 | works | adaptation |  |
| Behringer | Pro-800 | alpha | adaptation | Thanks to @Andy2No |
| Behringer | RD-8 | in progress | adaptation |  |
| Behringer | RD-9 | in progress | adaptation |  |
| Behringer | Wave | works | adaptation | Thanks to @willxy! |
| Black Corporation | Kijimi | beta | adaptation | Thanks to @ffont and @markusschlosser |
| DSI | Evolver | beta | adaptation |  |
| DSI | Mopho | works | adaptation |  |
| DSI | Mopho X4 | works | adaptation |  |
| DSI | Tetra | works | adaptation |  |
| DSI | Pro 2 | works | adaptation |  |
| DSI | Prophet 8 | works | adaptation |  |
| DSI | Tempest | alpha | adaptation |  |
| DSI/Sequential | OB-6 | works | native |  |
| DSI/Sequential | Prophet Rev2 | works | native |  |
| DSI/Sequential | Prophet 12 | works | adaptation | Thanks to @Andy2No |
| Electra | one | works | adaptation |  |
| Elektron | Analog Rytm | beta | adaptation | Thanks to @RadekPilich for the request! |
| Elektron | Digitone | alpha | adaptation | This needs more work, owners please provide feedback so we can complete it. |
| E-mu | Morpheus | works | adaptation | Thanks to Kid Who for testing! |
| Ensoniq | ESQ-1/SQ-80 | works | adaptation | Contributed by @Mostelin! |
| Ensoniq | VFX/VFX-SD | works | adaptation | Thanks to @dancingdog for testing! |
| Groove Synthesis | 3rd Wave | works | adaptation |  |
| John Bowen | Solaris | beta | adaptation | Contributed by @conversy! |
| Kawai | K1/K1m/K1r | beta | adaptation |  |
| Kawai | K3/K3m | works | native |  |
| Kawai | K4 | alpha | adaptation |  |
| Korg | 03R/W | works | adaptation | Thanks to Philippe! |
| Korg | DW-6000 | works | adaptation |  |
| Korg | DW-8000/EX-8000 | works | adaptation |  |
| Korg | M1 | works | adaptation | Thanks to Jentusalentu at YT for giving the nudge |
| Korg | microKORG S | works | adaptation | Thanks to @ilantz! |
| Korg | Minilogue XD | works | adaptation | Thanks to @andy2no |
| Korg | MS2000/microKORG | works | adaptation | Thanks to @windo |
| Line 6 | POD Series | works | adaptation | Thanks to @milnak! |
| Moog | Voyager | works | adaptation | Thanks to @troach242 for the nudge and test! |
| Novation | AStation/KStation | beta | adaptation | Thanks to @thechildofroth |
| Novation | Bass Station II | works | adaptation | Thanks to @cockroach! |
| Novation | Summit/Peak | alpha | adaptation |  |
| Novation | UltraNova | works | adaptation | Thanks to @nezetic |
| Oberheim | Matrix 6/6R | works | adaptation | Thanks to @tsantilis |
| Oberheim | Matrix 1000 | works | native |  |
| Oberheim | OB-X (Encore) | alpha | adaptation |  |
| Oberheim | OB-Xa (Encore) | alpha | adaptation |  |
| Oberheim | OB-8 | beta | adaptation |  |
| Oberheim | OB-X8 | beta | adaptation | help needed! |
| Pioneer | Toraiz AS-1 | works | adaptation | Thanks to @zzort! |
| Roland | JX-8P | alpha | adaptation |  |
| Roland | Juno-DS | works | adaptation | contributed by @mslinn! Thank you! |
| Roland | D-50 | works | adaptation | Shout out to @summersetter for testing! |
| Roland | JV-80/880/90/1000 | beta | adaptation |  |
| Roland | JV-1080/2080 | beta | adaptation |  |
| Roland | MKS-50 | alpha | adaptation |  |
| Roland | MKS-70 (Vecoven) | beta | adaptation | Thanks to @markusschloesser! |
| Roland | MKS-80 | works | native |  |
| Roland | V-Drums TD-07 | alpha | adaptation |  |
| Roland | XV-3080/5080/5050 | works | adaptation |  |
| Sequential | Pro 3 | works | adaptation |  |
| Sequential | Prophet-5 Rev 4 | works | adaptation |  |
| Sequential | Prophet-6 | beta | adaptation |  |
| Sequential | Prophet X | works | adaptation |  |
| Sequential | Take 5 | beta | adaptation |  |
| Sequential | Trigon-6 | works | adaptation |  |
| Studiologic | Sledge | beta | adaptation |  |
| Waldorf | Blofeld | beta | adaptation |  |
| Waldorf | M | works | adaptation | Thanks to @RadekPilich for testing! |
| Waldorf | MicroWave 1 | beta | adaptation | Thanks to Gerome S! |
| Waldorf | Kyra | alpha | adaptation | Thanks to Edisyn! |
| Waldorf | Pulse | works | adaptation | Thanks to @markusschlosser and chatGPT! |
| Yamaha | DX7 | beta | adaptation |  |
| Yamaha | DX7II | works | adaptation | Thanks to @AgtSlick for testing and the fixes! |
| Yamaha | FS1R | alpha | adaptation | Thanks to @markusschlosser for testing! |
| Yamaha | reface DX | works | adaptation |  |
| Yamaha | reface CP | beta | adaptation | Thanks to @milnak! |
| Yamaha | TX7 | works | adaptation | Thanks to Gerome S! |
| Yamaha | TX81Z | works | adaptation | Contributed by @summersetter! |
| Yamaha | Yamaha YC61/YC73/YC88 | works | adaptation | Thanks to @milnak! |
| Zoom | MS Series (50G/60B/70CDR) | works | adaptation | Thanks to @nezetic |
<!-- END:SUPPORTED_SYNTHS -->

If a synth is missing, or a status needs updating, open an issue (and ideally include test data).
