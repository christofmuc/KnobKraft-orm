#
#   Copyright (c) 2026 Rolf. All rights reserved.
#   KnobKraft Orm Adaptation for Korg X3 / X2 (Compact Baseline)
#
#   NOTE / WORKAROUND (Bulk import vs. Bank import):
#   - Bulk imports ("By import") load patches as a flat list without 
#     fixed bank and slot mapping. Clicking these sends an edit buffer dump 
#     to the synth (`0x40`/`0x49`). The Korg X3 receives the data in memory, 
#     but does not physically switch program locations on its display because 
#     the bank select/program change context is missing.
#   - WORKAROUND: Always import bank-specific files directly into the 
#     respective virtual banks (Program A, Program B, Combi A, Combi B) in the 
#     tree structure. This preserves their slot mapping (0-99) and causes 
#     the synth to respond immediately with a physical bank and program change 
#     upon each click.
#

from typing import List
import knobkraft
import testing

PROG_SIZE, COMBI_SIZE = 164, 136
PROG_COUNT, COMBI_COUNT = 200, 200

# Houd de laatst opgevraagde bank bij voor de 1-argument parser
_last_requested_bank = 0

def name(): return "Korg X3"
def deviceDetectWaitMilliseconds(): return 500
def generalMessageDelay(): return 400
def needsChannelSpecificDetection(): return True

def createDeviceDetectMessage(ch):
    return [0xf0, 0x7e, ch & 0x0f, 0x06, 0x01, 0xf7]

def channelIfValidDeviceResponse(msg):
    if len(msg) > 9 and msg[:2] == [0xf0, 0x7e] and msg[3:10] == [0x06, 0x02, 0x42, 0x35, 0x00, 0x00, 0x00]:
        return msg[2] & 0x0f
    return -1

def _x3_hdr(ch, fn): 
    return [0xf0, 0x42, 0x30 | (ch & 0x0f), 0x35, fn]

def _is_x3(msg):
    return (isinstance(msg, list) and len(msg) >= 6 and 
            msg[0] == 0xf0 and msg[1] == 0x42 and 
            (msg[2] & 0xf0) == 0x30 and msg[3] == 0x35 and msg[-1] == 0xf7)

def messageTimings():
    return {
        "bankDumpTimeoutMs": 20000,
        "editBufferTimeoutMs": 5000
    }

# ---------------------------------------------------------------------------
# SysEx Packing
# ---------------------------------------------------------------------------

def unescapeSysex(sysex):
    res, idx = [], 0
    while idx < len(sysex):
        msb = sysex[idx]; idx += 1
        for i in range(7):
            if idx < len(sysex):
                res.append(sysex[idx] | ((msb & (1 << i)) << (7 - i)))
            idx += 1
    return res

def escapeSysex(data):
    res = []
    for i in range(0, len(data), 7):
        chunk = data[i:i+7]
        msb = sum(((b & 0x80) >> (7 - j)) for j, b in enumerate(chunk))
        res.append(msb)
        res.extend(b & 0x7f for b in chunk)
    return res

# ---------------------------------------------------------------------------
# Edit Buffer Logic
# ---------------------------------------------------------------------------

def createEditBufferRequest(ch):
    return _x3_hdr(ch, 0x12) + [0xf7]

def _valid_dump(msg):
    if not _is_x3(msg) or len(msg) < 7 or msg[4] not in (0x40, 0x49): 
        return False
    raw_len = len(unescapeSysex(msg[5:-1]))
    return (msg[4] == 0x40 and raw_len == PROG_SIZE) or (msg[4] == 0x49 and raw_len == COMBI_SIZE)

def isPartOfEditBufferDump(msg):
    if not _is_x3(msg) or len(msg) < 7: return False
    
    if msg[4] == 0x42:  # MODE DATA response
        mode = msg[5] & 0x07
        req_cmd = 0x19 if mode in (0, 1) else 0x10  # 0x19 Combi, 0x10 Program fallback
        print("Korg X3 MODE DATA: mode=0x{:02X}; requesting {}".format(mode, "COMBI" if req_cmd == 0x19 else "PROGRAM"))
        return (False, _x3_hdr(msg[2] & 0x0f, req_cmd) + [0xf7])
        
    return _valid_dump(msg)

def isEditBufferDump(data):
    if isinstance(data, list) and data:
        if not isinstance(data[0], list): return _valid_dump(data)
        return any(_valid_dump(m) for m in data)
    return False

def convertToEditBuffer(ch, msg):
    if not _valid_dump(msg): raise Exception("Invalid dump structure")
    res = list(msg)
    res[2] = 0x30 | (ch & 0x0f)
    return res

# ---------------------------------------------------------------------------
# Bank Storage
# ---------------------------------------------------------------------------

def numberOfBanks(): return 4
def numberOfPatchesPerBank(): return 100

def bankDescriptors():
    return [
        {"bank": 0, "name": "Program A", "size": 100, "type": "Patch", "isROM": False},
        {"bank": 1, "name": "Program B", "size": 100, "type": "Patch", "isROM": False},
        {"bank": 2, "name": "Combi A", "size": 100, "type": "Combi", "isROM": False},
        {"bank": 3, "name": "Combi B", "size": 100, "type": "Combi", "isROM": False},
    ]

def createBankDumpRequest(ch, bank):
    global _last_requested_bank
    if bank not in (0, 1, 2, 3): raise Exception("Invalid bank: {}".format(bank))
    _last_requested_bank = bank
    return _x3_hdr(ch, 0x1C if bank in (0, 1) else 0x1D) + [0x00, 0xf7]

def isPartOfBankDump(msg):
    return len(msg) > 6 and _is_x3(msg) and msg[4] in (0x4C, 0x4D) and msg[5] == 0x00

def isBankDumpFinished(msgs):
    return any(isPartOfBankDump(m) for m in msgs)

def extractPatchesFromBank(msg):
    global _last_requested_bank
    if not isPartOfBankDump(msg): raise Exception("Invalid bank dump")
    ch = msg[2] & 0x0f
    is_prog = (msg[4] == 0x4C)
    rec_size = PROG_SIZE if is_prog else COMBI_SIZE
    opcode = 0x40 if is_prog else 0x49
    
    raw = unescapeSysex(msg[6:-1])
    if len(raw) != rec_size * PROG_COUNT: raise Exception("Bank size mismatch")

    # Slice 200-patch bulk dump op basis van de laatst opgevraagde bank
    start_idx = (_last_requested_bank % 2) * 100
    res = []
    for i in range(100):
        idx = start_idx + i
        rec = raw[idx * rec_size : (idx + 1) * rec_size]
        res.extend(_x3_hdr(ch, opcode) + escapeSysex(rec) + [0xf7])
    return res

# ---------------------------------------------------------------------------
# Patch Name & Selection
# ---------------------------------------------------------------------------

def nameFromDump(msg):
    if not _valid_dump(msg): return "Invalid"
    raw_name = unescapeSysex(msg[5:-1])[:10]
    parsed = ''.join(chr(x) if 32 <= x <= 126 else ' ' for x in raw_name).strip()
    return parsed if parsed else "Untitled"

def renamePatch(msg, new_name):
    if not _valid_dump(msg): raise Exception("Invalid dump")
    raw = unescapeSysex(msg[5:-1])
    clean = [ord(c) & 0x7f for c in str(new_name).ljust(10)[:10]]
    return msg[:5] + escapeSysex(clean + raw[10:]) + [0xf7]

def createCustomProgramChange(ch, patchNo):
    ch &= 0x0f
    if not (0 <= patchNo < 400): raise Exception("Invalid patch number")
    is_prog = patchNo < 200
    rel = patchNo if is_prog else patchNo - 200
    mode = 0x02 if is_prog else 0x00
    return [
        0xf0, 0x42, 0x30 | ch, 0x35, 0x4e, mode, 0x00, 0xf7,
        0xb0 | ch, 0x00, 0x00,
        0xb0 | ch, 0x20, 0 if rel < 100 else 1,
        0xc0 | ch, rel % 100
    ]

def bankSelect(ch, bank):
    if bank not in (0, 1, 2, 3): return []
    ch &= 0x0f
    return [
        0xf0, 0x42, 0x30 | ch, 0x35, 0x4e, 0x02 if bank in (0, 1) else 0x00, 0x00, 0xf7,
        0xb0 | ch, 0x00, 0x00,
        0xb0 | ch, 0x20, bank % 2
    ]

def friendlyBankName(bank):
    return {0: "Program A", 1: "Program B", 2: "Combi A", 3: "Combi B"}.get(bank, "X3")

def setupHelp():
    return "Korg X3: Memory Protect OFF. Global MIDI Channel must match KnobKraft."

# ---------------------------------------------------------------------------
# Test Data Generator
# ---------------------------------------------------------------------------

def make_test_data():
    raw = []
    for i in range(200):
        p = [0x20] * PROG_SIZE
        p[:10] = [ord(c) for c in "Prog {:03d}".format(i)]
        raw.extend(p)
    synthetic = [0xf0, 0x42, 0x30, 0x35, 0x4c, 0x00] + escapeSysex(raw) + [0xf7]
    
    def programs(data):
        patches = knobkraft.splitSysex(extractPatchesFromBank(data.all_messages[0]))
        yield testing.ProgramTestData(message=patches[0], name="Prog 000")
        yield testing.ProgramTestData(message=patches[50], name="Prog 050")

    return testing.TestData(
        sysex=synthetic,
        edit_buffer_generator=programs,
        bank_generator=lambda d: [d.all_messages],
        expected_patch_count=100
    )
