#include "doctest/doctest.h"

#include "GenericAdaptation.h"
#include "The-Orm/PatchSelectionEvents.h"

#include <pybind11/embed.h>
#include <algorithm>

namespace py = pybind11;

TEST_CASE("patch hooks are optional and receive a copy of the patch with its channel") {
	py::scoped_interpreter python;
	auto synth = knobkraft::GenericAdaptation::fromBinaryCode("patch_hooks", R"(
def name(): return "Patch hook test"
def onPatchSelected(channel, message):
    assert channel == -1
    assert message == [0xf0, 0x7d, 1, 0xf7]
    message[2] = 99
    return [0xc0, 7, 0xf0, 0x7d, 2, 0xf7]
def onPatchSent(channel, message):
    assert channel == 2
    assert message == [0xf0, 0x7d, 1, 0xf7]
    return [0xb1, 0, 2, 0xc1, 42]
)");
	REQUIRE(synth);
	const std::vector<uint8> patch{ 0xf0, 0x7d, 1, 0xf7 };
	auto selected = synth->onPatchSelected(MidiChannel::invalidChannel(), patch);
	REQUIRE(selected.size() == 2);
	CHECK(selected[0].getProgramChangeNumber() == 7);
	CHECK(selected[1].isSysEx());
	CHECK(patch[2] == 1);
	auto sent = synth->onPatchSent(MidiChannel::fromZeroBase(2), patch);
	REQUIRE(sent.size() == 2);
	CHECK(sent[0].isController());
	CHECK(sent[1].getChannel() == 2); // Hook output channel is not rewritten to the synth channel.
	CHECK(sent[1].getProgramChangeNumber() == 42);

	auto legacy = knobkraft::GenericAdaptation::fromBinaryCode("legacy_without_patch_hooks", "def name(): return 'Legacy'");
	REQUIRE(legacy);
	CHECK(legacy->onPatchSelected(MidiChannel::fromZeroBase(0), patch).empty());
	CHECK(legacy->onPatchSent(MidiChannel::fromZeroBase(0), patch).empty());
}

TEST_CASE("invalid hook results are rejected as a whole and Python errors do not poison subsequent hooks") {
	py::scoped_interpreter python;
	const std::vector<std::string> invalidResults{
		"[0xc0, 1, 0xf0, 0x7d]", // Valid first message followed by unfinished SysEx.
		"[0xc0]", "[0x90, 60]", "[0xc0, 128]", "[0xf0, 0xf7]",
		"[0xf0, 0x7d, 0x90, 0xf7]", "[0xf7]", "[0xf4]", "[7, 8]",
		"[256]", "[-1]", "'not MIDI'", "{'synth': [0xc0, 1]}"
	};
	for (const auto& result : invalidResults) {
		INFO(result);
		auto synth = knobkraft::GenericAdaptation::fromBinaryCode("invalid_patch_hook",
			"def name(): return 'Invalid hook'\ndef onPatchSelected(channel, message): return " + result
			+ "\ndef onPatchSent(channel, message): return [0xc0, 9]\n");
		REQUIRE(synth);
		CHECK(synth->onPatchSelected(MidiChannel::fromZeroBase(0), {}).empty());
		CHECK(synth->onPatchSent(MidiChannel::fromZeroBase(0), {}).size() == 1);
	}
	auto throwing = knobkraft::GenericAdaptation::fromBinaryCode("throwing_patch_hook", R"(
def name(): return "Throwing hook"
def onPatchSelected(channel, message): raise ValueError("broken hook")
def onPatchSent(channel, message): return [0xc0, 9]
)");
	REQUIRE(throwing);
	CHECK(throwing->onPatchSelected(MidiChannel::fromZeroBase(0), {}).empty());
	CHECK(throwing->onPatchSent(MidiChannel::fromZeroBase(0), {}).size() == 1);
	CHECK(PyErr_Occurred() == nullptr);
}

TEST_CASE("empty hook results are no-ops and hooks are discoverable in host API 3") {
	py::scoped_interpreter python;
	auto synth = knobkraft::GenericAdaptation::fromBinaryCode("empty_patch_hooks", R"(
import sys
assert sys._knobkraft_adaptation_api_version >= 3
def name(): return "Empty hooks"
def onPatchSelected(channel, message): pass
def onPatchSent(channel, message): return []
)");
	REQUIRE(synth);
	CHECK(synth->onPatchSelected(MidiChannel::fromZeroBase(0), {}).empty());
	CHECK(synth->onPatchSent(MidiChannel::fromZeroBase(0), {}).empty());
	for (const auto* hook : { "onPatchSelected", "onPatchSent" }) {
		CHECK(std::find_if(knobkraft::kAdaptationPythonFunctionNames.begin(), knobkraft::kAdaptationPythonFunctionNames.end(),
			[hook](const char* name) { return std::string(name) == hook; }) != knobkraft::kAdaptationPythonFunctionNames.end());
	}
}

TEST_CASE("selection notifications retain their patch snapshot and sent notifications require success") {
	py::scoped_interpreter python;
	auto synth = knobkraft::GenericAdaptation::fromBinaryCode("patch_event_dispatch", R"(
import sys
sys._patch_hook_calls = []
def name(): return "Dispatch test"
def onPatchSelected(channel, message):
    assert channel == 4
    assert message == [0xf0, 0x7d, 1, 0xf7]
    sys._patch_hook_calls.append("selected")
def onPatchSent(channel, message):
    assert channel == 4
    assert message == [0xf0, 0x7d, 1, 0xf7]
    sys._patch_hook_calls.append("sent")
)");
	REQUIRE(synth);
	auto calls = py::module_::import("sys").attr("_patch_hook_calls").cast<py::list>();
	std::vector<uint8> patch{ 0xf0, 0x7d, 1, 0xf7 };
	PatchSelectionEvents notifications(synth, MidiChannel::fromZeroBase(4), patch);
	patch[2] = 99;
	CHECK(calls.empty());
	notifications.selected(); // Browsing without sending still calls onPatchSelected.
	REQUIRE(calls.size() == 1);
	CHECK(calls[0].cast<std::string>() == "selected");
	using Status = midikraft::UploadResult::Status;
	for (auto status : { Status::DEVICE_ERROR, Status::TIMEOUT, Status::CANCELLED, Status::TRANSPORT_ERROR, Status::ADAPTATION_ERROR, Status::BUSY }) {
		notifications.sent({ status, {}, {} });
		CHECK(calls.size() == 1);
	}
	notifications.sent({ Status::ACKNOWLEDGED, {}, {} });
	REQUIRE(calls.size() == 2);
	CHECK(calls[1].cast<std::string>() == "sent");
	notifications.sent({ Status::SENT_WITHOUT_ACKNOWLEDGEMENT, {}, {} });
	CHECK(calls.size() == 3);
	synth.reset();
	notifications.sent({ Status::ACKNOWLEDGED, {}, {} });
	CHECK(calls.size() == 3); // Pending notification does not retain a destroyed synth.
}
