# Yamaha SY and TG Series SysEx Parameter Comparisons


| Parameter | SY22 | SY35 | TG33 | SY55 | TG55 | SY77 | TG77 | SY85 | TG500 | SY99 | SY99 Additional Voice Data | NOTES |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| synth_id (si) | 0x7e | 0x7e | 0x7e | 0x7a | 0x7a | 0x7a | 0x7a | 0x7a | 0x7a | 0x7a | 0x7a | |
| msg_id_bank_dump (bk) | PK__2203VM | PK__2203VM | LM__0012VC | -- | -- | -- | -- | -- | -- | -- | -- |  |
| msg_id_voice_dump (vo) | PK__2203AE | PK__2203AE | LM__0012VE | LM__8103VC | LM__8103VC | LM__8101VC | LM__8101VC | LM__0065VC | LM__0065VC | LM__8101VC | LM__0040VC | |
| **SYSEX PARAMETER OFFSETS** |  |  |  |  |  |  |  |  |  |  |  | |
| offset_memory_type | n/a | n/a | n/a | 30 | 30 | 30 | 30 | 30 | 30 | 30 | 30 | |
| offset_memory_number | n/a | n/a | n/a | 31 | 31 | 31 | 31 | 31 | 31 | 31 | 31 | |
| offset_req_memory_type | n/a | n/a | n/a | 28 | 28 | 28 | 28 | 28 | 28 | 28 | 28 | |
| offset_req_memory_number | n/a | n/a | n/a | 29 | 29 | 29 | 29 | 29 | 29 | 29 | 29 | |
| offset_voice_name | 19 | 19 | 20 | 33 | 33 | 33 | 33 | 105 | 105 | 33 | n/a | |
| voice_name_length | 8 | 8 | 8 | 10 | 10 | 10 | 10 | 8 | 8 | 10 | n/a | |
| voice_default_name | Initial | Initial | Initial | INIT VOICE | INIT VOICE | INIT VOICE | INIT VOICE | Init Vce | Init Vce | INIT VOICE | n/a | |
| first_preset_name | "Genesis_" | "AP:Rock_" | "SP*Pro33" | "Piano_____" | "Piano_____" | "GrandPiano" | "SP\|Cosmo__" | n/a | "AP Grand" | "AP\|Rocks__" | -- | |
| **SYSEX MESSAGE FORMATS** |  |  |  |  |  |  |  |  |  |  |  | |
| Header (hdr) | f0 43 0n SI | f0 43 0n SI | f0 43 0n SI | f0 43 0n SI | f0 43 0n SI | f0 43 0n SI | f0 43 0n SI | f0 43 0n SI | f0 43 0n SI | f0 43 0n SI |  | |
| 1 Voice Data Dump Request | [hdr] [vo] f7 | [hdr] [vo] f7 | [hdr] [vo] f7 | [hdr] [vo] [0 x14] mem_type mem_num f7 | [hdr] [vo] [0 x14] mem_type mem_num f7 | [hdr] [vo] [0 x14] mem_type mem_num f7 | [hdr] [vo] [0 x14] mem_type mem_num f7 | [hdr] [vo] [0 x14] mem_type mem_num f7 | [hdr] [vo] [0 x14] mem_type mem_num f7 | [hdr] [vo] [0 x14] mem_type mem_num f7 | [hdr] [vo] [0 x14] mem_type mem_num f7 | |
| 64 Voice Data Dump Request | [hdr] bk f7 | [hdr] bk f7 | [hdr] bk f7 | n/a | n/a | n/a | n/a | n/a | n/a | n/a | -- | |
|  |  |  |  |  |  |  |  |  |  |  |  | |
| 1 Voice Data Dump | [hdr] [bc] [vo] [voice data] CS f7 | [hdr] [bc] [vo] [voice data] CS f7 | [hdr] [bc] [vo] [voice data] CS f7 | [hdr] [bc] [vo] [0 x16] f7 | [hdr] [bc] [vo] [0 x16] f7 | [hdr] [bc] [vo] [0 x16] f7 | [hdr] [bc] [vo] [0 x16] f7 | [hdr] [bc] [vo] [0 x16] f7 | [hdr] [bc] [vo] [0 x16] f7 | [hdr] [bc] [vo] [0 x16] f7 | [hdr] [bc] [vo] [0 x16] f7 | Retrieves edit buffer if no `memory_type` and `memory_number` params are available |
| 64 Voice Data Dump | [hdr] [bc] bk [voice data x4 CS] [100ms] ... f7 | [hdr] [bc] bk [voice data x4 CS] [100ms] ... f7 | [hdr] [bc] bk [voice data x4 CS] [100ms] ... f7 | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | |
| Memory Types | n/a | n/a | INTERNAL:0 | INTERNAL:0 | INTERNAL:0 | INTERNAL:0 | INTERNAL:0 | INTERNAL1:0 | INTERNAL1:0 | INTERNAL:0 | INTERNAL:0 | |
|  |  |  | PRESET1:2 | PRESET:2 | PRESET:2 | PRESET1:2 | PRESET1:2 | INTERNAL2:3 | INTERNAL2:3 | PRESET1:2 | PRESET1:2 | |
|  |  |  | PRESET2:3 | EDIT:7F | EDIT:7F | PRESET2:3 | PRESET2:3 | INTERNAL3:6 | EDIT:7F | PRESET2:3 | PRESET2:3 | |
|  |  |  | EDIT:7F |  |  | EDIT:7F | EDIT:7F | INTERNAL4:9 |  | EDIT:7F | EDIT:7F | |
|  |  |  |  |  |  |  |  | EDIT:7F |  |  |  | |

Notes:
- `msg_id_bank_dump`, `msg_id_voice_dump`, and `first_preset_name` values use spaces in the specs, not underscores `_`.
  - The underscores are used here only to prevent line breaks in the markdown.
- [bc] = 2 byte payload size encoded as 7-bit MSB and 7-bit LSB
- CS = 7 bit checksum, code snippet below
- 1 Voice Data Dump Request
  - For SY22/35 and TG33 - acts on the edit buffer only
  - For others: can request any voice


## Device Specifics

- SY55/TG55 disambiguation
  - The TG55 has 2 output pairs, the SY55 has one.
  - MIDI Parameter Change Format for Voice Common messages has an `Output Select` (parameter 0x21)
  - SY55 only transmits a value of 0
  - TG55 can be set to values of 0-4. A change request can be sent and then read via sysex. If it's not changed to non-zero the device is an SY55
  - A similar parameter is available for Drum Voices and Multis.
  - Changing parameters/patches is not an acceptable way to detect devices. Implement a combined adaptation.
- SY85/TG500 disambiguation
  - TG500 has presets, SY85 doesn't
  - Needs multi-step device detection
- SY99
  - Sends `Additional Voice Data` with the `LM__0040VC` Voice Dump message ID.
  - It's not clear how this works without seeing SysEx captures.

## Code Snippets

```python
def calculateChecksum(self, data: List[int]) -> int:
    data_sum = sum(data[6:-2]) & 0x7f    # sum & mask to 7 bit values
    checksum = (0x80 - data_sum) & 0x7f  # subtract from 128 (0x80), mask again for 7-bit value
    return checksum
```

```python
def getPayloadSize(self, buf: List[int]) -> int:
    return (buf[OFFSET_SIZE_BYTE1] << 7) + buf[OFFSET_SIZE_BYTE2]
```
