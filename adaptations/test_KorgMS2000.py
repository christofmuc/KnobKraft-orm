import KorgMS2000


def testEscaping():
    testData = [x for x in range(254)]
    escaped = KorgMS2000.escapeSysex(testData)
    back = KorgMS2000.unescapeSysex(escaped)
    assert testData == back


def test_clear_text_uses_unpacked_payload_offsets():
    payload = [0x80, 0x01, 0xff, 0x7f, 0, 0x42, 0xa5, 0x11, 0x22]
    message = [0xf0, 0x42, 0x30, 0x58, 0x40] + KorgMS2000.escapeSysex(payload) + [0xf7]
    original = message.copy()
    assert KorgMS2000.getClearText(message) == [
        ("Unescaped patch data", "0000 80 01 ff 7f 00 42 a5 11\n0008 22")
    ]
    assert message == original
    assert KorgMS2000.getClearText([]) == []
