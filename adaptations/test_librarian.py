from testing.librarian import Librarian


def program_dump(slot: int, sound: int):
    return [0xF0, 0x01, slot, sound, 0xF7]


class ProgramStreamBankAdaptation:
    @staticmethod
    def createProgramDumpRequest(channel, patch_no):
        return []

    @staticmethod
    def isSingleProgramDump(message):
        return len(message) == 5 and message[1] == 0x01

    @staticmethod
    def isPartOfBankDump(message):
        return ProgramStreamBankAdaptation.isSingleProgramDump(message)

    @staticmethod
    def isBankDumpFinished(messages):
        return len(messages) == 3

    @staticmethod
    def extractPatchesFromAllBankMessages(messages):
        return messages

    @staticmethod
    def calculateFingerprint(message):
        return str(message[3])


def test_load_sysex_deduplicates_bank_stream_of_program_dumps():
    messages = [program_dump(0, 10), program_dump(1, 11), program_dump(2, 12)]

    patches = Librarian().load_sysex(ProgramStreamBankAdaptation, messages)

    assert patches == messages


class PartiallyOverlappingBankAdaptation(ProgramStreamBankAdaptation):
    @staticmethod
    def isPartOfBankDump(message):
        return True

    @staticmethod
    def isBankDumpFinished(messages):
        return len(messages) == 2

    @staticmethod
    def extractPatchesFromAllBankMessages(messages):
        return [program_dump(0, 10), program_dump(1, 10)]


def test_load_sysex_deduplicates_bank_patches_by_occurrence():
    messages = [program_dump(0, 10), [0xF0, 0x02, 0xF7]]

    patches = Librarian().load_sysex(PartiallyOverlappingBankAdaptation, messages)

    assert patches == [program_dump(0, 10), program_dump(1, 10)]


class MutatingFingerprintBankAdaptation:
    @staticmethod
    def isPartOfBankDump(message):
        return True

    @staticmethod
    def isBankDumpFinished(messages):
        return len(messages) == 1

    @staticmethod
    def extractPatchesFromAllBankMessages(messages):
        return [program_dump(0, 10)]

    @staticmethod
    def calculateFingerprint(message):
        message[2] = 99
        return str(message)


def test_load_sysex_preserves_bank_patch_when_fingerprint_mutates_input():
    patches = Librarian().load_sysex(MutatingFingerprintBankAdaptation, [[0xF0, 0x02, 0xF7]])

    assert patches == [program_dump(0, 10)]
