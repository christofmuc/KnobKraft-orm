# Nord Lead regression fixtures

These existing fixtures come from Nord's official download archives:

| Files | Source | Verified contents |
| --- | --- | --- |
| `bank0.syx`, `bank1.syx` | [Nord Lead 2X factory bank](https://www.nordkeyboards.com/wt/documents/797/Nord%20Lead%202x%20Factory%20Bank%20v1.00%20revA.zip) | Each contains 99 normal programs and 10 percussion kits; kits are deliberately excluded from import. |
| `nord-lead-2-internal.mid` | [Nord Lead 2 factory bank](https://www.nordkeyboards.com/wt/documents/708/Nord%20Lead%202%20Factory%20Bank.zip) | 40 normal programs. |
| `nord-lead-bank1.mid` | [Nord Lead sound cards 1–4](https://www.nordkeyboards.com/wt/documents/706/Nord%20Lead%20Sound%20Card%201-4.zip) | 99 normal programs. |

The sound data is unchanged by the release-hardening work. These are manufacturer
files, not newly recorded hardware captures or newly generated public-domain sounds;
the source attribution does not grant a new license to the sound data.

## Protocol evidence and limits

The [Nord Lead 2X manual, edition 1.1](https://www.nordkeyboards.com/wt/documents/218/Nord%20Lead%202x%20English%20User%20Manual%20v1.0%20Edition%201.1.pdf)
documents ten program-dump banks on printed page 106, but only four program-request
banks (`0x0B`–`0x0E`) on pages 107–108. Page 109 assigns `0x14` to All Controllers
Request, so extrapolating request types across ten banks is unsafe. The adaptation
therefore exposes four live banks while accepting ten-bank program files. Actual
bank availability depends on the model and installed memory card.

The regression suite relocates fixture payloads to independently constructed
addresses, including the higher 2X banks. These are simulated responses, not
hardware evidence. Tests cover nonzero channels, all four live banks, repeated
bank downloads, temporary Slot A audition, fingerprints, rejected percussion and
malformed dumps, and complete/fragmented/escaped SysEx in MIDI files. Malformed
MIDI tracks fail with `ValueError`, without returning partially parsed patches.

Hardware detection, timing, writable-memory behavior, and requests for higher 2X
banks remain unverified. Performances, percussion kits, and Nord Lead 3+ are outside
this adaptation's scope. Keep the adaptation marked alpha until hardware testing.
