"""Generate original R3 protocol fixtures without importing the adaptation.

Copyright (c) 2026 Christof Ruch. All rights reserved.
Dual licensed: Affero GPL by default; an MIT license is available for purchase.
The generated data is distributed under the same terms as this repository.
See README.md for format sources and limitations.
"""

from pathlib import Path


def payload(name, seed):
    # Deliberately exercise all byte values, rather than model a playable sound.
    data = bytearray((seed + i * 17) % 256 for i in range(456))
    data[:8] = name.encode("ascii").ljust(8, b" ")
    # Known bit-order vectors and the one-byte final group from the packing rule.
    data[14:21] = bytes([0x80, 0x01, 0x82, 0x03, 0x84, 0x05, 0x86])
    data[448:456] = bytes([0x80, 0x01, 0xfe, 0x7f, 0x84, 0x05, 0x86, 0x80])
    packed = bytearray()
    for offset in range(0, 456, 7):
        group = data[offset:offset + 7]
        packed.append(sum((value // 128) * (2 ** bit) for bit, value in enumerate(group)))
        packed.extend(value % 128 for value in group)
    assert len(packed) == 522
    assert packed[16:24] == bytes.fromhex("55 00 01 02 03 04 05 06")
    assert packed[-10:] == bytes.fromhex("55 00 01 7e 7f 04 05 06 01 00")
    return packed


def generate():
    messages = []
    for number, name, seed in [(0, "BassSaw", 0x11), (1, "HyperVoc", 0x22),
                               (7, "Screamin", 0x33), (8, "Justice", 0x44)]:
        messages.append(bytes([0xf0, 0x42, 0x30, 0x7d, 0x4c, number, 0])
                        + payload(name, seed) + b"\xf7")
    messages.append(bytes.fromhex("f0 42 30 7d 40") + payload("EditR3", 0x55) + b"\xf7")
    assert [len(message) for message in messages] == [530, 530, 530, 530, 528]
    return b"".join(messages)


if __name__ == "__main__":
    Path(__file__).with_name("synthetic.syx").write_bytes(generate())
