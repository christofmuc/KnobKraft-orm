#include "Midnam.h"


void saveToMidnam(String const& filename, std::shared_ptr<midikraft::SynthBank> currentBank) {

	// Write an XML file
	XmlElement root("MIDINameDocument");
	root.createNewChildElement("Author")->addTextElement("The KnobKraft Orm Sysex Librarian");
	auto master = root.createNewChildElement("MasterDeviceNames");
	String synthName(currentBank->synth()->getName());
	auto manufacturer = synthName.upToFirstOccurrenceOf(" ", false, true);
	auto deviceName = synthName.fromFirstOccurrenceOf(" ", false, true);
	master->createNewChildElement("Manufacturer")->addTextElement(manufacturer);
	master->createNewChildElement("Model")->addTextElement(deviceName);

	// Custom Device Mode
	auto custom = master->createNewChildElement("CustomDeviceMode");
	custom->setAttribute("Name", "Default");
	auto assignments = custom->createNewChildElement("ChannelNameSetAssignments");
	for (int i = 0; i < 16; i++) {
		auto assign = assignments->createNewChildElement("ChannelNameSetAssign");
		assign->setAttribute("Channel", i + 1);
		assign->setAttribute("NameSet", "Bank");
	}

	// ChannelNameSet
	auto channelSet = master->createNewChildElement("ChannelNameSet");
	channelSet->setAttribute("Name", "Bank");
	auto availability = channelSet->createNewChildElement("AvailableForChannels");
	for (int i = 0; i < 16; i++) {
		auto available = availability->createNewChildElement("AvailableChannel");
		available->setAttribute("Channel", i + 1);
		available->setAttribute("Available", "true");
	}

	// Now the bank 
	auto bank = channelSet->createNewChildElement("PatchBank");
	bank->setAttribute("Name", currentBank->name());
	bank->setAttribute("ROM", "false");  // TODO - we could check if this is a ROM bank, couldn't we?
	auto reference = bank->createNewChildElement("UsesPatchNameList");
	reference->setAttribute("Name", currentBank->name());

	// Now the patch list
	auto patchList = master->createNewChildElement("PatchNameList");
	for (auto const& patch : currentBank->patches()) {
		auto patchEntry = patchList->createNewChildElement("Patch");
		patchEntry->setAttribute("Number", patch.smartSynth()->friendlyProgramName(patch.patchNumber()));
		patchEntry->setAttribute("Name", patch.name());
		patchEntry->setAttribute("ProgramChange", patch.patchNumber().toZeroBasedDiscardingBank());
	}

	XmlElement::TextFormat format;
	format.dtd = "<!DOCTYPE MIDINameDocument PUBLIC \" -//MIDI Manufacturers Association//DTD MIDINameDocument 1.0//EN\" \"http://www.midi.org/dtds/MIDINameDocument10.dtd\">";
	File output(filename);
	root.writeTo(output, format);
}
