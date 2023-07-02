import pytest


def is_valid_sysex(message):
    # Check that message begins with 0xf0 and ends with 0xf7
    if message[0] != 0xf0 or message[-1] != 0xf7:
        return False

    # Check that all bytes within the message body do not have the high bit set
    for byte in message[1:-1]:
        if byte & 0x80 != 0:
            return False

    return True


@pytest.fixture
def valid_sysex():
    return is_valid_sysex
