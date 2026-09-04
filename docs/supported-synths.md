# Supported Synths

Find your exact model, then check its setup help in **Settings** and the manufacturer's MIDI instructions. A family entry is not a claim about every similarly named instrument. Support for importing a file does not establish every live request, audition or bank-writing capability.

## Status and release availability

- **Works:** established support reported by the project; capabilities still vary by model.
- **Beta:** usable support that needs broader testing.
- **Alpha:** early support; inspect the notes and expect limitations.
- **In progress:** not included in regular builds.

This list reflects source reviewed on **4 September 2026**. The release column distinguishes the checked **2.9.0** release from **After 2.9.0** additions. Newer source is not a promise that a feature is in your installed build. The MKS-50 Python replacement, SE-02 PRM loader and Pro 3 bank/pacing updates are also newer than 2.9.0; older builds have different capabilities for those models.

Use the <a href="../../#checker">searchable model filter</a>, or choose a manufacturer below. For connection steps, start with [Connect your synth](learn/connect.md); for device-specific detail, consult the [community device wiki](https://github.com/christofmuc/KnobKraft-orm/wiki/Device-index). Wiki pages vary in completeness and age, so compare them with your installed adaptation's help. [Report a correction or successful test](help.md#ask-for-help-or-report-a-problem).

<!-- BEGIN:SUPPORTED_SYNTHS_TABLE -->
## Manufacturers

- [Access](#manufacturer-access)
- [Akai](#manufacturer-akai)
- [Alesis](#manufacturer-alesis)
- [Behringer](#manufacturer-behringer)
- [Black Corporation](#manufacturer-black-corporation)
- [Casio](#manufacturer-casio)
- [Clavia](#manufacturer-clavia)
- [DSI](#manufacturer-dsi)
- [DSI/Sequential](#manufacturer-dsi-sequential)
- [Elektron](#manufacturer-elektron)
- [E-mu](#manufacturer-e-mu)
- [Ensoniq](#manufacturer-ensoniq)
- [Groove Synthesis](#manufacturer-groove-synthesis)
- [John Bowen](#manufacturer-john-bowen)
- [Kawai](#manufacturer-kawai)
- [Korg](#manufacturer-korg)
- [Line 6](#manufacturer-line-6)
- [Moog](#manufacturer-moog)
- [Novation](#manufacturer-novation)
- [Oberheim](#manufacturer-oberheim)
- [Pioneer](#manufacturer-pioneer)
- [Roland](#manufacturer-roland)
- [Sequential](#manufacturer-sequential)
- [Studiologic](#manufacturer-studiologic)
- [Waldorf](#manufacturer-waldorf)
- [Yamaha](#manufacturer-yamaha)
- [Zoom](#manufacturer-zoom)

<a id="manufacturer-access"></a>
## Access

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| Virus A, B, Classic, KB, Indigo | works | native | 2.9.0 |  |
| Virus C | beta | native | 2.9.0 | Thanks to guavadude@gs |

<a id="manufacturer-akai"></a>
## Akai

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| AX80 | beta | adaptation | 2.9.0 | Thanks to O.S.R.C. on YT for the nudge |

<a id="manufacturer-alesis"></a>
## Alesis

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| Andromeda A6 | works | adaptation | 2.9.0 | Thanks to @markusschloesser |

<a id="manufacturer-behringer"></a>
## Behringer

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| BCR2000 | in progress | native | 2.9.0 |  |
| Deepmind 12 | works | adaptation | 2.9.0 |  |
| Pro-800 | alpha | adaptation | 2.9.0 | Thanks to @Andy2No |
| RD-8 | in progress | adaptation | Not in regular builds |  |
| RD-9 | in progress | adaptation | Not in regular builds |  |
| UB-Xa | alpha | adaptation | After 2.9.0 | Contributed by @Casuallynoted; hardware validation needed |
| Wave | works | adaptation | 2.9.0 | Thanks to @willxy! |

<a id="manufacturer-black-corporation"></a>
## Black Corporation

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| Kijimi | beta | adaptation | 2.9.0 | Thanks to @ffont and @markusschlosser |

<a id="manufacturer-casio"></a>
## Casio

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| CZ-101/CZ-1000 | alpha | adaptation | After 2.9.0 | Contributed by @Casuallynoted; hardware validation needed |

<a id="manufacturer-clavia"></a>
## Clavia

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| Nord Lead / Nord Lead 2 / Nord Lead 2X | alpha | adaptation | After 2.9.0 | Four documented live-request banks (availability depends on model and installed memory card); ten-bank 2X file import/export; official fixtures and mock MIDI tested, hardware verification pending; no Nord Lead 3+ |

<a id="manufacturer-dsi"></a>
## DSI

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| Evolver | beta | adaptation | 2.9.0 |  |
| Mopho | works | adaptation | 2.9.0 |  |
| Mopho X4 | works | adaptation | 2.9.0 |  |
| Tetra | works | adaptation | 2.9.0 |  |
| Pro 2 | works | adaptation | 2.9.0 |  |
| Prophet 8 | works | adaptation | 2.9.0 |  |
| Tempest | alpha | adaptation | 2.9.0 |  |

<a id="manufacturer-dsi-sequential"></a>
## DSI/Sequential

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| OB-6 | works | native | 2.9.0 |  |
| Prophet Rev2 | works | native | 2.9.0 |  |
| Prophet 12 | works | adaptation | 2.9.0 | Thanks to @Andy2No |

<a id="manufacturer-elektron"></a>
## Elektron

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| Analog Rytm | beta | adaptation | 2.9.0 | Thanks to @RadekPilich for the request! |
| Digitone | alpha | adaptation | 2.9.0 | This needs more work, owners please provide feedback so we can complete it. |

<a id="manufacturer-e-mu"></a>
## E-mu

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| Morpheus | works | adaptation | 2.9.0 | Thanks to Kid Who for testing! |

<a id="manufacturer-ensoniq"></a>
## Ensoniq

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| ESQ-1/SQ-80 | works | adaptation | 2.9.0 | Contributed by @Mostelin! |
| Mirage (SoundProcess) | alpha | adaptation | After 2.9.0 | Contributed by @Casuallynoted; alternate SysEx header needs hardware validation |
| VFX/VFX-SD | works | adaptation | 2.9.0 | Thanks to @dancingdog for testing! |

<a id="manufacturer-groove-synthesis"></a>
## Groove Synthesis

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| 3rd Wave | works | adaptation | 2.9.0 |  |

<a id="manufacturer-john-bowen"></a>
## John Bowen

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| Solaris | beta | adaptation | 2.9.0 | Contributed by @conversy! |

<a id="manufacturer-kawai"></a>
## Kawai

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| K1/K1m/K1r | beta | adaptation | 2.9.0 |  |
| K3/K3m | works | native | 2.9.0 |  |
| K4 | alpha | adaptation | 2.9.0 |  |
| K5000 | beta | adaptation | 2.9.0 | Heroic effort by @markusschlosser! Most complex! |

<a id="manufacturer-korg"></a>
## Korg

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| 03R/W | works | adaptation | 2.9.0 | Thanks to Philippe! |
| DW-6000 | works | adaptation | 2.9.0 |  |
| DW-8000/EX-8000 | works | adaptation | 2.9.0 |  |
| M1 | works | adaptation | 2.9.0 | Thanks to Jentusalentu at YT for giving the nudge |
| microKORG S | works | adaptation | 2.9.0 | Thanks to @ilantz! |
| Minilogue XD | works | adaptation | 2.9.0 | Thanks to @andy2no |
| MS2000/microKORG | works | adaptation | 2.9.0 | Thanks to @windo |
| R3 | alpha | adaptation | 2.9.0 | 100% AI generated |

<a id="manufacturer-line-6"></a>
## Line 6

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| POD Series | works | adaptation | 2.9.0 | Thanks to @milnak! |

<a id="manufacturer-moog"></a>
## Moog

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| Voyager | works | adaptation | 2.9.0 | Thanks to @troach242 for the nudge and test! |

<a id="manufacturer-novation"></a>
## Novation

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| AStation/KStation | beta | adaptation | 2.9.0 | Thanks to @thechildofroth |
| Bass Station II | works | adaptation | 2.9.0 | Thanks to @cockroach! |
| Summit/Peak | alpha | adaptation | 2.9.0 |  |
| UltraNova | works | adaptation | 2.9.0 | Thanks to @nezetic |

<a id="manufacturer-oberheim"></a>
## Oberheim

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| Matrix 6/6R | works | adaptation | 2.9.0 | Thanks to @tsantilis |
| Matrix 1000 | works | native | 2.9.0 |  |
| OB-X (Encore) | alpha | adaptation | 2.9.0 |  |
| OB-Xa (Encore) | alpha | adaptation | 2.9.0 |  |
| OB-8 | beta | adaptation | 2.9.0 |  |
| OB-X8 | beta | adaptation | 2.9.0 | help needed! |

<a id="manufacturer-pioneer"></a>
## Pioneer

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| Toraiz AS-1 | works | adaptation | 2.9.0 | Thanks to @zzort! |

<a id="manufacturer-roland"></a>
## Roland

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| JX-8P | alpha | adaptation | 2.9.0 |  |
| Juno-DS | works | adaptation | 2.9.0 | contributed by @mslinn! Thank you! |
| D-50 | works | adaptation | 2.9.0 | Shout out to @summersetter for testing! |
| JD-Xi | alpha | adaptation | 2.9.0 | 100% AI generated |
| JV-80/880/90/1000 | beta | adaptation | 2.9.0 |  |
| JV-1080/2080 | beta | adaptation | 2.9.0 |  |
| MKS-50 | alpha | adaptation | 2.9.0 |  |
| MKS-70 (Vecoven) | beta | adaptation | 2.9.0 | Thanks to @markusschloesser! |
| MKS-80 | works | native | 2.9.0 |  |
| SE-02 | beta | adaptation | 2.9.0 | DT1 dumps and USB backup PRM import (MammaScan converter mapping). Thanks to @MammaScan! |
| U-20/U-220 | alpha | adaptation | 2.9.0 | 100% AI generated. Thanks to @Casuallynoted for testing! |
| V-Drums TD-07 | alpha | adaptation | 2.9.0 |  |
| XV-3080/5080/5050 | works | adaptation | 2.9.0 |  |

<a id="manufacturer-sequential"></a>
## Sequential

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| Fourm | beta | adaptation | After 2.9.0 | Real dump tested; hardware verification pending |
| Pro 3 | works | adaptation | 2.9.0 | User/factory bank metadata and transfer pacing thanks to @RadekPilich |
| Prophet-5 Rev 4 | works | adaptation | 2.9.0 |  |
| Prophet-6 | beta | adaptation | 2.9.0 |  |
| Prophet X | works | adaptation | 2.9.0 |  |
| Take 5 | beta | adaptation | 2.9.0 |  |
| Trigon-6 | works | adaptation | After 2.9.0 |  |

<a id="manufacturer-studiologic"></a>
## Studiologic

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| Sledge | beta | adaptation | 2.9.0 |  |

<a id="manufacturer-waldorf"></a>
## Waldorf

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| Blofeld | beta | adaptation | 2.9.0 |  |
| M | works | adaptation | 2.9.0 | Thanks to @RadekPilich for testing! |
| MicroWave 1 | beta | adaptation | 2.9.0 | Thanks to Gerome S! |
| Kyra | alpha | adaptation | 2.9.0 | Thanks to Edisyn! |
| Pulse | works | adaptation | 2.9.0 | Thanks to @markusschlosser and chatGPT! |

<a id="manufacturer-yamaha"></a>
## Yamaha

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| DX7 | beta | adaptation | 2.9.0 |  |
| DX7II | works | adaptation | 2.9.0 | Thanks to @AgtSlick for testing and the fixes! |
| FS1R | alpha | adaptation | 2.9.0 | Thanks to @markusschlosser for testing! |
| reface DX | works | adaptation | 2.9.0 |  |
| reface CP | beta | adaptation | 2.9.0 | Thanks to @milnak! |
| TX7 | works | adaptation | 2.9.0 | Thanks to Gerome S! |
| TX81Z | works | adaptation | 2.9.0 | Contributed by @summersetter! |
| Yamaha YC61/YC73/YC88 | works | adaptation | 2.9.0 | Thanks to @milnak! |

<a id="manufacturer-zoom"></a>
## Zoom

| Synth | Status | Type | Release | Notes / thanks |
| --- | --- | --- | --- | --- |
| MS Series (50G/60B/70CDR) | works | adaptation | 2.9.0 | Thanks to @nezetic |
<!-- END:SUPPORTED_SYNTHS_TABLE -->
