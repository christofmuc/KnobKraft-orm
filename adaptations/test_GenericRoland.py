"""Regressions for #560–562, using wire addresses as the independent oracle."""
import copy
import itertools
from pathlib import Path

import pytest

import knobkraft
import Roland_JV80
import Roland_JV1080
import Roland_JD_Xi
import Roland_XV3080
from roland import DataBlock, GenericRoland, GenericRolandWithBackwardCompatibility, RolandData
from testing.librarian import Librarian


def flatten(blocks):
    return list(itertools.chain.from_iterable(blocks))


@pytest.fixture(params=['jv80', 'jv1080', 'jdxi', 'xv3080'])
def sound(request):
    if request.param == 'jv80':
        model = copy.deepcopy(Roland_JV80.jv_80)
        patch = next(iter(Roland_JV80.make_test_data().programs)).message.byte_list
    elif request.param == 'jv1080':
        model = copy.deepcopy(Roland_JV1080.jv_1080)
        patch = next(iter(Roland_JV1080.make_test_data().programs)).message.byte_list
    elif request.param == 'jdxi':
        model = copy.deepcopy(Roland_JD_Xi._jdxi_sn_tone)
        patch = next(iter(Roland_JD_Xi.make_test_data().edit_buffers)).message.byte_list
    else:
        model = copy.deepcopy(Roland_XV3080.xv_3080_main)
        # Extracted unchanged from the existing test_Roland_XV3080 fixture.
        patch = list(Path('testData/Roland_XV3080/Pianomonics.syx').read_bytes())
    model.device_id = 0x10
    return model, patch


def payloads(model, patch):
    start = 4 + len(model.model_id)
    return {tuple(block[start:start + model.address_size]): block[start + model.address_size:-2]
            for block in knobkraft.splitSysex(patch)}


def rewrite(model, block, *, address=None, data=None, device=None):
    start = 4 + len(model.model_id)
    address = block[start:start + model.address_size] if address is None else address
    data = block[start + model.address_size:-2] if data is None else data
    # Deliberately independent of buildRolandMessage / parseRolandMessage.
    header = block[:start]
    if device is not None:
        header[2] = device
    return header + address + data + [(-sum(address + data)) & 127, 247]


def test_permutations_preserve_addresses_and_metadata(sound):
    model, patch = sound
    blocks = knobkraft.splitSysex(patch)
    expected_edit = payloads(model, model.convertToEditBuffer(0, patch))
    expected_program = payloads(model, model.convertToProgramDump(0, patch, 7))
    expected_name = model.nameFromDump(patch)
    expected_tags = model.storedTags(patch)
    expected_hash = model.calculateFingerprint(patch)
    # All permutations for five-block models; all equal-sized XV tone permutations
    # plus reversal (moves common/name/category to the end).
    if len(blocks) <= 5:
        permutations = itertools.permutations(blocks)
    else:
        permutations = [list(reversed(blocks))] + [blocks[:5] + list(p) for p in itertools.permutations(blocks[5:])]
    for permutation in permutations:
        reordered = flatten(permutation)
        assert model.isSingleProgramDump(reordered) or model.isEditBufferDump(reordered)
        assert payloads(model, model.convertToEditBuffer(11, reordered)) == expected_edit
        assert payloads(model, model.convertToProgramDump(11, reordered, 7)) == expected_program
        assert model.nameFromDump(reordered) == expected_name
        assert model.storedTags(reordered) == expected_tags
        assert model.calculateFingerprint(reordered) == expected_hash
        renamed = model.renamePatch(reordered, 'RENAMED')
        assert model.nameFromDump(renamed) == 'RENAMED'.ljust(model.patch_name_length)
        before, after = payloads(model, reordered), payloads(model, renamed)
        name_address = tuple(blocks[model.patch_name_message_number][4 + len(model.model_id):
                                                                      4 + len(model.model_id) + model.address_size])
        assert {address for address in before if before[address] != after[address]} == {name_address}
        assert before[name_address][:model.patch_name_offset] == after[name_address][:model.patch_name_offset]
        name_end = model.patch_name_offset + model.patch_name_length
        assert before[name_address][name_end:] == after[name_address][name_end:]


@pytest.mark.parametrize('edit', [False, True])
def test_fingerprint_invariants_and_parameter_sensitivity(sound, edit):
    model, patch = sound
    expected = model.calculateFingerprint(patch)
    patch = model.convertToEditBuffer(0, patch) if edit else model.convertToProgramDump(0, patch, 0)
    original = patch.copy()
    for device in (0, 0x10, 0x15, 0x1f):
        routed = flatten(rewrite(model, b, device=device) for b in knobkraft.splitSysex(patch))
        assert model.calculateFingerprint(routed) == expected
        assert model.calculateFingerprint(model.renamePatch(routed, 'NEW NAME')) == expected
    for place in (0, model.program_dump.num_items - 1):
        assert model.calculateFingerprint(model.convertToProgramDump(3, patch, place)) == expected
    # Exercise every payload byte, including the byte before each checksum.
    blocks = knobkraft.splitSysex(patch)
    for block_no, block in enumerate(blocks):
        data = block[4 + len(model.model_id) + model.address_size:-2]
        for offset in range(len(data)):
            if block_no == model.patch_name_message_number and model.patch_name_offset <= offset < model.patch_name_offset + model.patch_name_length:
                continue
            changed = data.copy()
            changed[offset] ^= 1
            mutated = blocks.copy()
            mutated[block_no] = rewrite(model, block, data=changed)
            assert model.calculateFingerprint(flatten(mutated)) != expected, (block_no, offset)
    assert patch == original


@pytest.mark.parametrize('edit', [False, True])
@pytest.mark.parametrize('damage', ['model', 'command', 'checksum', 'short', 'long', 'duplicate',
                                  'conflicting_duplicate', 'unknown', 'missing', 'device', 'program',
                                  'prefix', 'suffix', 'between', 'truncated', 'nested', 'high_bit', 'empty'])
def test_malformed_dumps_are_rejected(sound, edit, damage):
    model, patch = sound
    patch = model.convertToEditBuffer(0, patch) if edit else model.convertToProgramDump(0, patch, 0)
    blocks = knobkraft.splitSysex(patch)
    address_start = 4 + len(model.model_id)
    if damage == 'model':
        blocks[-1][3] ^= 1
    elif damage == 'command':
        blocks[-1][address_start - 1] = 0x11
    elif damage == 'checksum':
        blocks[-1][-2] ^= 1
    elif damage in ('short', 'long'):
        data = blocks[-1][address_start + model.address_size:-2]
        blocks[-1] = rewrite(model, blocks[-1], data=data[:-1] if damage == 'short' else data + [0])
    elif damage in ('duplicate', 'conflicting_duplicate'):
        duplicate = blocks[0].copy()
        if damage == 'conflicting_duplicate':
            data = duplicate[address_start + model.address_size:-2]
            data[-1] ^= 1
            duplicate = rewrite(model, duplicate, data=data)
        blocks.append(duplicate)
    elif damage == 'unknown':
        address = blocks[-1][address_start:address_start + model.address_size]
        address[-1] = 1
        blocks[-1] = rewrite(model, blocks[-1], address=address)
    elif damage == 'missing':
        blocks.pop()
    elif damage == 'device':
        blocks[-1][2] = 0x15
    elif damage == 'program':
        address = blocks[-1][address_start:address_start + model.address_size]
        address[1] += 1
        blocks[-1] = rewrite(model, blocks[-1], address=address)
    elif damage == 'prefix':
        blocks.insert(0, [0])
    elif damage == 'suffix':
        blocks.append([0])
    elif damage == 'between':
        blocks.insert(1, [0])
    elif damage == 'truncated':
        blocks[-1].pop()
    elif damage == 'nested':
        blocks[0].insert(address_start, 0xf0)
    elif damage == 'high_bit':
        blocks[-1][-3] = 0x80
        blocks[-1][-2] = (-sum(blocks[-1][address_start:-2])) & 127
    else:
        blocks = []
    malformed = flatten(blocks)
    assert not model.isSingleProgramDump(malformed)
    assert not model.isEditBufferDump(malformed)
    assert model.nameFromDump(malformed) == 'Invalid'
    assert model.storedTags(malformed) == []
    for operation in (lambda: model.convertToEditBuffer(0, malformed),
                      lambda: model.convertToProgramDump(0, malformed, 0),
                      lambda: model.renamePatch(malformed, 'NO'),
                      lambda: model.calculateFingerprint(malformed)):
        with pytest.raises(ValueError):
            operation()


def test_partial_recognition_validates_each_block(sound):
    model, patch = sound
    for edit in (True, False):
        converted = model.convertToEditBuffer(0, patch) if edit else model.convertToProgramDump(0, patch, 0)
        predicate = model.isPartOfEditBufferDump if edit else model.isPartOfSingleProgramDump
        for block in knobkraft.splitSysex(converted):
            assert predicate(block)
            for end in range(len(block)):
                assert not predicate(block[:end])
            bad = block.copy()
            bad[-2] ^= 1
            assert not predicate(bad)
            bad = block.copy()
            bad[4 + len(model.model_id) - 1] = 0x11
            assert not predicate(bad)


@pytest.mark.parametrize('module', [Roland_JV1080, Roland_XV3080])
def test_librarian_deduplicates_program_and_edit(module):
    patch = (next(iter(module.make_test_data().programs)).message.byte_list if module is Roland_JV1080
             else list(Path('testData/Roland_XV3080/Pianomonics.syx').read_bytes()))
    program = module.convertToProgramDump(0, patch, 0)
    edit = module.renamePatch(module.convertToEditBuffer(0, patch), 'EDIT NAME')
    assert len(Librarian().load_sysex(module, knobkraft.splitSysex(program) + knobkraft.splitSysex(edit))) == 1


def test_legacy_program_hashes(sound):
    model, patch = sound
    # Captured from the unchanged pre-fix module, not computed by the new code.
    expected = {'Roland JV-80': 'b112b99735e8dae3b0ec0d8e28574d50',
                'Roland JV-1080': '3abd33edc2d6f03628583fdf86fb03aa',
                'Roland JD-Xi': '5a8ed265efbc6e018eeea59bba89bf87',
                'Roland XV-3080': '2a82aa6220f30d91e944bebb2666b99e'}
    assert model.calculateFingerprint(patch) == expected[model.name()]


@pytest.mark.parametrize('module,sizes', [(Roland_JV80, (0x22, 0x73, 0x73, 0x73, 0x73)),
                                       (Roland_JV80, (0x22, 0x74, 0x74, 0x74, 0x74)),
                                       (Roland_JV80, (0x22, 0x75, 0x75, 0x75, 0x75)),
                                       (Roland_JV1080, (0x48, 0x81, 0x81, 0x81, 0x81)),
                                       (Roland_JV1080, (0x4a, 0x81, 0x81, 0x81, 0x81))])
def test_documented_jv_variants(module, sizes):
    model = module.jv_80 if module is Roland_JV80 else module.jv_1080
    original = next(iter(module.make_test_data().programs)).message.byte_list
    blocks = knobkraft.splitSysex(original)
    resized = []
    for block, size in zip(blocks, sizes):
        data = block[9:-2]
        resized.append(rewrite(model, block, data=(data + [17, 23])[:size]))
    patch = flatten(resized)
    assert model.isSingleProgramDump(patch)
    edit = model.convertToEditBuffer(0, flatten(reversed(resized)))
    assert model.isEditBufferDump(edit)
    assert model.convertToProgramDump(0, edit, model.numberFromDump(patch)) == patch
    assert model.calculateFingerprint(edit) == model.calculateFingerprint(patch)
    assert model.calculateFingerprint(model.renamePatch(edit, 'VARIANT')) == model.calculateFingerprint(patch)
    # A byte appended by the variant is sound data too.
    resized[0][-3] ^= 1
    resized[0] = rewrite(model, resized[0])
    assert model.calculateFingerprint(flatten(resized)) != model.calculateFingerprint(patch)


def test_mixed_jv_tone_lengths_rejected():
    patch = next(iter(Roland_JV80.make_test_data().programs)).message.byte_list
    blocks = knobkraft.splitSysex(patch)
    blocks[1] = rewrite(Roland_JV80.jv_80, blocks[1], data=blocks[1][9:-2] + [0])
    assert Roland_JV80.isPartOfSingleProgramDump(blocks[1])
    assert not Roland_JV80.isSingleProgramDump(flatten(blocks))


def test_xv_destination_state_is_local_and_used_by_all_paths():
    sources = [Roland_XV3080.xv_3080_main, Roland_JV80.jv_80, Roland_JV1080.jv_1080]
    initial_ids = [model.device_id for model in sources]
    wrappers = [GenericRolandWithBackwardCompatibility(sources[0], sources[1:]) for _ in range(2)]
    patches = [list(Path('testData/Roland_XV3080/Pianomonics.syx').read_bytes()),
               next(iter(Roland_JV80.make_test_data().programs)).message.byte_list,
               next(iter(Roland_JV1080.make_test_data().programs)).message.byte_list]
    for index, device in [(0, 0x15), (1, 0x1f), (0, 0x10), (0, 0x1f), (1, 0x15)]:
        wrapper = wrappers[index]
        other_device = wrappers[1 - index].main_model.device_id
        reply = [0xf0, 0x7e, device, 6, 2, 0x41, 0x10, 1, 0, 0, 0, 0, 0, 0, 0xf7]
        assert wrapper.channelIfValidDeviceResponse(reply) == device & 15
        assert wrapper.createEditBufferRequest(7)[2] == device
        assert wrapper.createProgramDumpRequest(7, 5)[2] == device
        for patch in patches:
            for converted, predicate in [(wrapper.convertToEditBuffer(7, patch), wrapper.isPartOfEditBufferDump),
                                         (wrapper.convertToProgramDump(7, patch, 5), wrapper.isPartOfSingleProgramDump)]:
                for block in knobkraft.splitSysex(converted):
                    assert block[2] == device
                    # Compatible JV imports retain their source protocol.
                    assert block[3] == patch[3]
                    accepted, followup = predicate(block)
                    assert accepted
                    if followup:
                        assert followup[2] == device
        assert [model.device_id for model in sources] == initial_ids
        assert wrappers[1 - index].main_model.device_id == other_device


def test_public_xv_detection_then_send(monkeypatch):
    wrapper = Roland_XV3080.xv_3080
    for model in wrapper.models_supported:
        monkeypatch.setattr(model, 'device_id', 0x10)
    assert Roland_XV3080.channelIfValidDeviceResponse([240, 126, 21, 6, 2, 65, 16, 1, 0, 0, 0, 0, 0, 0, 247]) == 5
    patch = list(Path('testData/Roland_XV3080/Pianomonics.syx').read_bytes())
    assert all(block[2] == 21 for block in knobkraft.splitSysex(Roland_XV3080.convertToEditBuffer(5, patch)))
    assert all(block[2] == 21 for block in knobkraft.splitSysex(Roland_XV3080.convertToProgramDump(5, patch, 0)))


def test_name_offset_length_and_block_identity():
    blocks = [DataBlock((0, 0, 0), 32, 'Parameters'), DataBlock((0, 0, 32), 32, 'Name')]
    edit = RolandData('Edit', 1, 3, 3, (0, 0, 0), blocks)
    program = RolandData('Programs', 8, 3, 3, (1, 0, 0), blocks)
    model = GenericRoland('Offset name', [0x42], 3, edit, program,
                          patch_name_message_number=1, patch_name_offset=8, patch_name_length=16)
    patch = flatten(model.buildRolandMessage(16, 18, [1, 0, i * 32], [i + 1] * 32) for i in range(2))
    renamed = model.renamePatch(flatten(reversed(knobkraft.splitSysex(patch))), 'SIXTEEN CHARS!!!')
    assert model.nameFromDump(renamed) == 'SIXTEEN CHARS!!!'
    assert payloads(model, renamed)[(1, 0, 0)] == [1] * 32
    assert payloads(model, renamed)[(1, 0, 32)][:8] == [2] * 8
    assert payloads(model, renamed)[(1, 0, 32)][24:] == [2] * 8
    assert model.calculateFingerprint(renamed) == model.calculateFingerprint(patch)
    assert model.calculateFingerprint(model.convertToEditBuffer(0, renamed)) == model.calculateFingerprint(patch)


def test_consecutive_addresses_do_not_alias_other_programs_into_edit_buffer():
    blocks = [DataBlock((0, 0, 0), 256, 'A'), DataBlock((0, 2, 0), 128, 'B')]
    edit = RolandData('Edit', 1, 3, 3, (0, 0, 0), blocks, uses_consecutive_addresses=True)
    program = RolandData('Programs', 64, 3, 3, (5, 0, 0), blocks, uses_consecutive_addresses=True)
    model = GenericRoland('Consecutive', [0x3d], 3, edit, program, patch_name_length=16)
    patch = model.buildRolandMessage(16, 18, [5, 3, 0], [1] * 256) + model.buildRolandMessage(16, 18, [5, 5, 0], [2] * 128)
    assert model.numberFromDump(patch) == 1
    reordered = flatten(reversed(knobkraft.splitSysex(patch)))
    assert model.convertToProgramDump(0, reordered, 1) == patch
    assert model.calculateFingerprint(model.convertToEditBuffer(0, reordered)) == model.calculateFingerprint(patch)
    aliased_edit = model.buildRolandMessage(16, 18, [0, 3, 0], [1] * 256) + model.buildRolandMessage(16, 18, [0, 5, 0], [2] * 128)
    assert not model.isEditBufferDump(aliased_edit)


def test_ambiguous_layout_is_rejected():
    blocks = [DataBlock((0, 0, 0), 32, 'A'), DataBlock((0, 1, 0), 32, 'B')]
    layout = RolandData('Unsupported layout', 8, 3, 3, (1, 0, 0), blocks)
    model = GenericRoland('Ambiguous', [0x42], 3, layout, layout)
    patch = model.buildRolandMessage(16, 18, [1, 0, 0], [0] * 32)
    assert not model.isSingleProgramDump(patch)
    with pytest.raises(ValueError):
        model.convertToEditBuffer(0, patch)
