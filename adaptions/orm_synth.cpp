#include "Synth.h"
#include "Patch.h"

#include "EditBufferCapability.h"
#include "HasBanksCapability.h"
#include "ProgramDumpCapability.h"

#include <pybind11/stl.h>

namespace py = pybind11;
using namespace py::literals;
using namespace midikraft;

// Trampoline class to allow Python to override C++ virtual functions
// See https://pybind11.readthedocs.io/en/stable/advanced/classes.html
//
class PySynth : public Synth
{
public:
	using Synth::Synth;

	virtual std::string getName() const override
	{
		PYBIND11_OVERRIDE_PURE_NAME(
			std::string,
			NamedDeviceCapability,
			"get_name",
			getName,
			);
	}

	virtual std::shared_ptr<DataFile> patchFromPatchData(const Synth::PatchData& data, MidiProgramNumber place) const override
	{
		ignoreUnused(place);
		return std::make_shared<DataFile>(0, data);
	}

	// TODO this should go away
	virtual bool isOwnSysex(MidiMessage const& message) const override
	{
		ignoreUnused(message);
		return false;
	}

	virtual std::string friendlyProgramAndBankName(MidiBankNumber bankNo, MidiProgramNumber programNo) const override {
		PYBIND11_OVERRIDE_NAME(
			std::string, // Return type
			Synth, // Parent class
			"friendly_program_name", // Python function name
			friendlyProgramAndBankName, // C++ method name
			bankNo,
			programNo
		);
	}

	// Override this if you disagree with the default implementation of calculating the fingerprint with an md5 of the filtered patch data
	virtual std::string calculateFingerprint(std::shared_ptr<DataFile> patch) const override
	{
		PYBIND11_OVERRIDE_NAME(
			std::string,
			Synth,
			"calculate_fingerprint",
			calculateFingerprint,
			patch
		);
	}

	// Override this if you have some words for the user of this synth to properly do the manual setup steps that might be required for vintage gear
	virtual std::string setupHelpText() const override {
		PYBIND11_OVERRIDE_NAME(
			std::string,
			Synth,
			"setup_help_text",
			setupHelpText
		);
	}
};

class PyDiscoverableDevice : public DiscoverableDevice {
public:
	using DiscoverableDevice::DiscoverableDevice;

	virtual std::vector<juce::MidiMessage> deviceDetect(int channel) override
	{
		PYBIND11_OVERRIDE_PURE_NAME(
			std::vector<juce::MidiMessage>,
			DiscoverableDevice,
			"create_device_detect_message",
			deviceDetect,
			channel
		);
	}

	virtual int deviceDetectSleepMS() override
	{
		PYBIND11_OVERRIDE_PURE_NAME(
			int,
			DiscoverableDevice,
			"device_detect_sleep_ms",
			deviceDetectSleepMS,
			);
	}

	virtual MidiChannel channelIfValidDeviceResponse(const juce::MidiMessage& message) override
	{
		PYBIND11_OVERRIDE_PURE_NAME(
			MidiChannel,
			DiscoverableDevice,
			"channel_if_valid_device_detect_response",
			channelIfValidDeviceResponse,
			message
		);
	}

	virtual bool needsChannelSpecificDetection()
	{
		PYBIND11_OVERRIDE_PURE_NAME(
			bool,
			DiscoverableDevice,
			"needs_channel_specific_detection",
			needsChannelSpecificDetection,
			);
	}

	virtual bool endDeviceDetect(MidiMessage& endMessageOut) const
	{
		return false;
	}

	virtual bool wasDetected() const
	{
		return wasDetected_;
	}

	virtual void setWasDetected(bool wasDetected)
	{
		wasDetected_ = wasDetected;
	}

private:
	bool wasDetected_ = false;
};

class PyEditBufferCapability : public EditBufferCapability
{
public:
	using EditBufferCapability::EditBufferCapability;

	virtual std::vector<MidiMessage> requestEditBufferDump() const override
	{
		PYBIND11_OVERRIDE_PURE_NAME(
			std::vector<MidiMessage>,
			EditBufferCapability,
			"request_edit_buffer",
			requestEditBufferDump,
			);
	}

	virtual bool isEditBufferDump(const std::vector<MidiMessage>& messages) const override
	{
		PYBIND11_OVERRIDE_PURE_NAME(
			bool,
			EditBufferCapability,
			"is_edit_buffer",
			isEditBufferDump,
			messages
		);

	}

	virtual HandshakeReply isMessagePartOfEditBuffer(const MidiMessage& message) const override {
		PYBIND11_OVERRIDE_NAME(
			HandshakeReply,
			EditBufferCapability,
			"is_part_of_edit_buffer",
			isMessagePartOfEditBuffer,
			message
		);
	}

	virtual std::shared_ptr<DataFile> patchFromSysex(const std::vector<MidiMessage>& message) const override
	{
		py::gil_scoped_acquire acquire;
		// For the Generic Adaptation, this is a nop, as we do not unpack the MidiMessage, but rather store the raw MidiMessage
		midikraft::Synth::PatchData data;
		for (auto const& m : message) {
			std::copy(m.getRawData(), m.getRawData() + m.getRawDataSize(), std::back_inserter(data));
		}
		return std::make_shared<DataFile>(0, data);
	}

	virtual std::vector<MidiMessage> patchToSysex(std::shared_ptr<DataFile> patch) const override
	{
		PYBIND11_OVERRIDE_PURE_NAME(
			std::vector<MidiMessage>,
			EditBufferCapability,
			"convert_to_edit_buffer",
			patchToSysex,
			patch
		);
	}

	virtual MidiMessage saveEditBufferToProgram(int programNumber) override
	{
		PYBIND11_OVERRIDE_PURE_NAME(
			MidiMessage,
			EditBufferCapability,
			"save_edit_buffer",
			saveEditBufferToProgram,
			programNumber
		);
	}

};

class PyHasBankDescriptorsCapability : public HasBankDescriptorsCapability
{
public:
	using HasBankDescriptorsCapability::HasBankDescriptorsCapability;

	virtual std::vector<BankDescriptor> bankDescriptors() const override
	{
		PYBIND11_OVERRIDE_PURE_NAME(
			std::vector<BankDescriptor>,
			HasBankDescriptorsCapability,
			"bank_descriptors",
			bankDescriptors,
		);
	}

	virtual std::vector<juce::MidiMessage> bankSelectMessages(MidiBankNumber bankNo) const
	{
		PYBIND11_OVERRIDE_PURE_NAME(
			std::vector<juce::MidiMessage>,
			HasBankDescriptorsCapability,
			"bank_select",
			bankSelectMessages,
			bankNo
		);
	}
};

class PyProgramDumpCabability : public ProgramDumpCabability
{
public:
	using ProgramDumpCabability::ProgramDumpCabability;

	virtual std::vector<MidiMessage> requestPatch(int patchNo) const override
	{
		PYBIND11_OVERRIDE_PURE_NAME(
			std::vector<MidiMessage>,
			ProgramDumpCabability,
			"request_program_dump",
			requestPatch,
			patchNo
			);
	}

	virtual bool isSingleProgramDump(const std::vector<MidiMessage>& messages) const override
	{
		PYBIND11_OVERRIDE_PURE_NAME(
			bool,
			ProgramDumpCabability,
			"is_single_program_dump",
			isSingleProgramDump,
			messages
		);
	}

	virtual HandshakeReply isMessagePartOfProgramDump(const MidiMessage& message) const {
		PYBIND11_OVERRIDE_NAME(
			HandshakeReply,
			ProgramDumpCabability,
			"is_part_of_single_dump",
			isMessagePartOfProgramDump,
			message
		);
	}

	std::shared_ptr<midikraft::DataFile> patchFromProgramDumpSysex(const std::vector<MidiMessage>& message) const override
	{
		py::gil_scoped_acquire acquire;
		// For the Generic Adaptation, this is a nop, as we do not unpack the MidiMessage, but rather store the raw MidiMessage
		midikraft::Synth::PatchData data;
		for (auto const& m : message) {
			std::copy(m.getRawData(), m.getRawData() + m.getRawDataSize(), std::back_inserter(data));
		}
		return std::make_shared<DataFile>(0, data);
	}


	virtual MidiProgramNumber getProgramNumber(const std::vector<MidiMessage>& messages) const override
	{
		PYBIND11_OVERRIDE_PURE_NAME(
			MidiProgramNumber,
			ProgramDumpCabability,
			"number_from_dump",
			getProgramNumber,
			messages
		);
	}

	virtual std::vector<MidiMessage> patchToProgramDumpSysex(std::shared_ptr<DataFile> patch, MidiProgramNumber programNumber) const
	{
		PYBIND11_OVERRIDE_PURE_NAME(
			std::vector<MidiMessage>,
			ProgramDumpCabability,
			"convert_to_program_dump",
			patchToProgramDumpSysex,
			patch,
			programNumber
		);
	}
	
};


juce::MidiMessage midiMesssageFromBytes(std::vector<uint8_t> const& content)
{
	return juce::MidiMessage(content.data(), (int) content.size());
}

MidiBankNumber bankFromZeroBasedInt(int bank, int bankSize)
{
	return MidiBankNumber::fromZeroBase(bank, bankSize);
}

MidiProgramNumber programFromZeroBasedInt(MidiBankNumber bank, int program)
{
	return MidiProgramNumber::fromZeroBaseWithBank(bank, program);
}


PYBIND11_MODULE(orm_synth, m)
{
	py::class_<juce::MidiMessage>(m, "MidiMessage")
		.def(py::init(&midiMesssageFromBytes))
		.def("__repr__", [](MidiMessage const& message) { return "<" + message.getDescription().toStdString() + ">";  })
		.def("__len__", [](MidiMessage const& message) { return message.getRawDataSize(); })
		.def("__getitem__", [](MidiMessage const& message, int index) { return index < message.getRawDataSize() ? message.getRawData()[index] : 0;  })
		.def("__getitem__", [](MidiMessage const& message, py::slice slice) {
		py::ssize_t start, stop, step, slicelength;
		if (!slice.compute(message.getRawDataSize(), &start, &stop, &step, &slicelength)) {
			throw py::error_already_set();
		}
		std::vector<int> result;
		for (py::ssize_t i = 0; i < slicelength; ++i) {
			result.push_back(message.getRawData()[start]);
			start += step;
		}
		return result;
			})
		.def("is_sysex", [](MidiMessage const& message) { return message.isSysEx();  });

	py::class_<MidiBankNumber>(m, "Bank")
		.def(py::init(&bankFromZeroBasedInt))
		.def_static("from_zero_base", &MidiBankNumber::fromZeroBase)
		.def_static("from_one_base", &MidiBankNumber::fromOneBase)
		.def_static("invalid", &MidiBankNumber::invalid)
		.def("is_valid", &MidiBankNumber::isValid)
		.def("size", &MidiBankNumber::bankSize)
		.def("to_zero_base", &MidiBankNumber::toZeroBased)
		.def("to_one_base", &MidiBankNumber::toOneBased);

	py::class_<MidiProgramNumber>(m, "Program")
		.def(py::init(&programFromZeroBasedInt))
		.def("value", &MidiProgramNumber::toZeroBasedWithBank)
		.def_static("from_zero_base", &MidiProgramNumber::fromZeroBase)
		.def_static("from_one_base", &MidiProgramNumber::fromOneBase);

	py::class_<BankDescriptor>(m, "BankDescriptor")
		.def(py::init<MidiBankNumber, std::string, int, bool, std::string>(),
			py::arg("bank"), py::arg("name"), py::arg("size"), py::arg("is_rom"), py::arg("type"))
		.def_readwrite("bank", &BankDescriptor::bank)
		.def_readwrite("name", &BankDescriptor::name)
		.def_readwrite("size", &BankDescriptor::size)
		.def_readwrite("is_rom", &BankDescriptor::isROM)
		.def_readwrite("type", &BankDescriptor::type);

	py::class_<DataFile>(m, "Patch")
		.def(py::init<int>())
		.def("__len__", [](Patch const& patch) { return patch.data().size(); })
		.def("__getitem__", [](Patch const& patch, int index) { return index < patch.data().size() ? patch.data()[index] : 0;  });

	py::class_<Synth, PySynth>(m, "Synth")
		.def(py::init<>())
		.def("get_name", &Synth::getName)
		.def("friendly_program_name", &Synth::friendlyProgramAndBankName)
		.def("calculate_fingerprint", &Synth::calculateFingerprint)
		.def("setup_help_text", &Synth::setupHelpText);

	py::class_<DiscoverableDevice, PyDiscoverableDevice>(m, "DiscoverableDevice")
		.def(py::init<>())
		.def("create_device_detect_message", &DiscoverableDevice::deviceDetect)
		.def("device_detect_sleep_ms", &DiscoverableDevice::deviceDetectSleepMS)
		.def("channel_if_valid_device_detect_response", &DiscoverableDevice::channelIfValidDeviceResponse)
		.def("needs_channel_specific_detection", &DiscoverableDevice::needsChannelSpecificDetection);

	py::class_<EditBufferCapability, PyEditBufferCapability>(m, "EditBufferCapability")
		.def(py::init<>())
		.def("request_edit_buffer", &EditBufferCapability::requestEditBufferDump)
		.def("is_edit_buffer", &EditBufferCapability::isEditBufferDump)
		.def("is_part_of_edit_buffer", &EditBufferCapability::isMessagePartOfEditBuffer)
		.def("convert_to_edit_buffer", &EditBufferCapability::patchToSysex)
		.def("save_edit_buffer", &EditBufferCapability::saveEditBufferToProgram);

	py::class_<ProgramDumpCabability, PyProgramDumpCabability>(m, "ProgramDumpCapability")
		.def(py::init<>())
		.def("request_program_dump", &ProgramDumpCabability::requestPatch)
		.def("is_single_program_dump",  &ProgramDumpCabability::isSingleProgramDump)
		.def("is_part_of_program_dump", &ProgramDumpCabability::isMessagePartOfProgramDump)
		.def("number_from_dump", &ProgramDumpCabability::getProgramNumber)
		.def("convert_to_program_dump", &ProgramDumpCabability::patchToProgramDumpSysex)
		;

	py::class_<HasBankDescriptorsCapability, PyHasBankDescriptorsCapability>(m, "BankDescriptorsCapability")
		.def(py::init<>())
		.def("bank_descriptors", &HasBankDescriptorsCapability::bankDescriptors)
		.def("bank_select", &HasBankDescriptorsCapability::bankSelectMessages);
}
