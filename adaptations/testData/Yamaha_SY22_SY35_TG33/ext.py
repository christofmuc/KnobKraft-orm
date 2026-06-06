#!/usr/bin/env python3

msg_id_all_voice_dump = "PK  2203VM"
msg_id_voice_dump = "PK  2203AE"

SYSEX_START=0xF0
SYSEX_END=0xF7
YAMAHA_ID = 0x43
OFFSET_CHECKSUM=-2

def str2bytes(s: str):
    return [ord(x) for x in s]

def _calculateRawChecksum(data):
    data_sum = sum(data) & 0x7f  # sum & mask to 7 bit values
    checksum = (0x80 - data_sum) & 0x7f  # subtract from 128 (0x80), mask again for 7-bit value
    return checksum

def _calculateChecksum(data):
    return _calculateRawChecksum(data[6:-2])


def extractPatchesFromBank(bank):
    # check for "PK  2203VM" to ensure we've a bank message to process
    if msg_id_all_voice_dump is None:
        raise ValueError("Undefined: msg_id_all_voice_dump")
    if bank[6:16] != str2bytes(msg_id_all_voice_dump):
        raise ValueError("Not a all voice bank dump.")

    prefix = [SYSEX_START, YAMAHA_ID, 0, 0x7e, 0x04, 0x3e] + str2bytes(msg_id_voice_dump)
    suffix = [0, SYSEX_END]
    
    patches = []
    offset = 16
    patch_len = 574  # each voice is 574 bytes
    while len(patches) < 64: 
        # 16-589; 590-1163; 1164-1737; 1738-2312
        for x in range(4):
            patch = prefix + bank[offset:offset+patch_len-1] + suffix
            patch[OFFSET_CHECKSUM] = _calculateChecksum(patch)
            patches.append(patch)
            offset += patch_len
        # TODO: check checksum
        offset += 3  # skip checksum + 2 length bytes
    return patches


testfile = "TG33-Waves.syx"
with open(testfile, "rb") as fh:
    raw = list(fh.read())

patches = extractPatchesFromBank(raw)
for i, p in enumerate(patches):
    vname = "".join([chr(x) for x in p[19:27]])
    prefix = " ".join([f"{n:02x}" for n in p[:16]]) + f" .. {p[-2]:02x} {p[-1]:02x}"
    print(f"{i:02d} {vname} {prefix}")
