from pathlib import Path

import knobkraft
import Yamaha_TG33 as tg33
from testing.librarian import Librarian
from testing.mock_midi import MockMidiController

VOICE = knobkraft.load_sysex(str(Path(__file__).resolve().parent / "testData" / "Yamaha_TG33" / "edisyn-init-voice.syx"))[0]


class TG33:
    """A TG33 as Edisyn describes it: in Edit mode it ignores bank selects, the VOICE button leaves Edit mode,
    the bank is CC 0 then CC 32, and a request returns the current voice."""

    def __init__(self, channel, banks):
        self.channel, self.banks = channel, banks
        self.editing, self.bank, self.msb, self.program = True, 2, None, 0

    def respond(self, message):
        c = self.channel
        if message == [0xF0, 0x43, 0x10 | c, 0x26, 0x07, 0x06, 0xF7]:
            self.editing = False
        elif message[:2] == [0xB0 | c, 0] and not self.editing:
            self.msb = message[2]
        elif message[:2] == [0xB0 | c, 32] and not self.editing and self.msb == 0:
            self.bank = message[2]
        elif message[0] == 0xC0 | c:
            self.program = message[1]
        elif message == tg33.createEditBufferRequest(c):
            return [self.banks[self.bank][self.program]]
        else:
            raise AssertionError(f"unexpected {knobkraft.syxToString(message)}")
        return []


def test_a_bank_download_selects_the_internal_bank_first():
    internal = [tg33.renamePatch(VOICE, f"INT {i}") for i in range(64)]
    preset = [tg33.renamePatch(VOICE, f"PRE {i}") for i in range(64)]
    midi = MockMidiController(TG33(3, {0: internal, 2: preset}))
    downloaded = []
    Librarian().start_downloading_all_patches(midi, 3, tg33, 0, downloaded.extend)
    midi.drain()
    assert [tg33.nameFromDump(v) for v in downloaded] == [f"INT {i}".ljust(8) for i in range(64)]
    assert midi.sent_messages[:5] == [[0xF0, 0x43, 0x13, 0x26, 0x07, 0x06, 0xF7], [0xB3, 0, 0], [0xB3, 32, 0], [0xC3, 0],
                                      tg33.createEditBufferRequest(3)]
    # The Orm waits this long between the messages of one request, as Edisyn does after the button.
    assert tg33.messageTimings()["generalMessageDelay"] == 1000
