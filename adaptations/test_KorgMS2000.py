import KorgMS2000


def testEscaping():
    testData = [x for x in range(254)]
    escaped = KorgMS2000.escapeSysex(testData)
    back = KorgMS2000.unescapeSysex(escaped)
    assert testData == back
