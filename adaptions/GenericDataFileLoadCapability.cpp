/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "GenericDataFileLoadCapability.h"

#include "Sysex.h"

using namespace midikraft;

#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <boost/format.hpp>

namespace py = pybind11;


std::vector<midikraft::DataFileLoadCapability::DataFileDescription> knobkraft::GenericDataFileLoadCapability::dataTypeNames() const
{
	try {
		py::object result = me_->callMethod(kDataTypeIDs);
		std::map<std::string, int> types = py::cast<std::map<std::string, int>>(result);
		std::vector<midikraft::DataFileLoadCapability::DataFileDescription> returnvalue;
		for (auto t : types) {
			returnvalue.push_back({ DataFileType(t.second), t.first });
		}
		return returnvalue;
	}
	catch (py::error_already_set &ex) {
		me_->logAdaptationError(kDataTypeIDs, ex);
		ex.restore();
	}
	catch (std::exception &ex) {
		me_->logAdaptationError(kDataTypeIDs, ex);
	}
	return {};
}

std::vector<midikraft::DataFileLoadCapability::DataFileImportDescription> knobkraft::GenericDataFileLoadCapability::dataFileImportChoices() const
{
	try {
		py::object result = me_->callMethod(kDataRequestIds);
		std::map<std::string, std::pair<int, int>> types = py::cast<std::map<std::string, std::pair<int, int>>>(result);
		std::vector<midikraft::DataFileLoadCapability::DataFileImportDescription> returnvalue;
		for (auto t : types) {
			returnvalue.push_back({ DataStreamType(t.second.first), t.first, t.second.second });
		}
		return returnvalue;
	}
	catch (py::error_already_set &ex) {
		me_->logAdaptationError(kDataRequestIds, ex);
		ex.restore();
	}
	catch (std::exception &ex) {
		me_->logAdaptationError(kDataRequestIds, ex);
	}
	return {};
}

bool knobkraft::GenericDataFileLoadCapability::isDataFile(const MidiMessage &message, DataFileType dataTypeID) const
{
	ignoreUnused(message, dataTypeID);
	return false;
}


std::vector<juce::MidiMessage> knobkraft::GenericDataFileLoadCapability::requestDataItem(int itemNo, DataStreamType dataStreamType)
{
	try {
		int midiChannel = 0;
		auto location = dynamic_cast<midikraft::MidiLocationCapability *>(me_);
		if (location && location->channel().isValid()) {
			midiChannel = location->channel().toZeroBasedInt();
		}
		int intType = dataStreamType.asInt();
		py::object result = me_->callMethod(kCreateStreamRequest, midiChannel, intType, itemNo);
		std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(result.cast<std::vector<int>>());
		return Sysex::vectorToMessages(byteData);
	}
	catch (py::error_already_set &ex) {
		me_->logAdaptationError(kCreateStreamRequest, ex);
		ex.restore();
	}
	catch (std::exception &ex) {
		me_->logAdaptationError(kCreateStreamRequest, ex);
	}
	return {};
}

int knobkraft::GenericDataFileLoadCapability::numberOfMidiMessagesPerStreamType(DataStreamType dataTypeID) const
{
	try {
		int dataType = dataTypeID.asInt();
		py::object result = me_->callMethod(kMessagesPerStreamType, dataType);
		return py::cast<int>(result);
	}
	catch (py::error_already_set &ex) {
		me_->logAdaptationError(kMessagesPerStreamType, ex);
		ex.restore();
	}
	catch (std::exception &ex) {
		me_->logAdaptationError(kMessagesPerStreamType, ex);
	}
	return 0;
}

bool knobkraft::GenericDataFileLoadCapability::isPartOfDataFileStream(const MidiMessage &message, DataStreamType dataTypeID) const
{
	try {
		int dataType = dataTypeID.asInt();
		auto vector = me_->messageToVector(message);
		py::object result = me_->callMethod(kIsPartOfStream, dataType, vector);
		return py::cast<bool>(result);
	}
	catch (py::error_already_set &ex) {
		me_->logAdaptationError(kIsPartOfStream, ex);
		ex.restore();
	}
	catch (std::exception &ex) {
		me_->logAdaptationError(kIsPartOfStream, ex);
	}
	return false;
}

bool knobkraft::GenericDataFileLoadCapability::isStreamComplete(std::vector<MidiMessage> const &messages, DataStreamType streamType) const
{
	try {
		int dataType = streamType.asInt();
		std::vector<std::vector<int>> vector;
		for (auto message : messages) {
			vector.push_back(me_->messageToVector(message));
		}
		py::object result = me_->callMethod(kIsStreamComplete, dataType, vector);
		return py::cast<bool>(result);
	}
	catch (py::error_already_set &ex) {
		me_->logAdaptationError(kIsStreamComplete, ex);
		ex.restore();
	}
	catch (std::exception &ex) {
		me_->logAdaptationError(kIsStreamComplete, ex);
	}
	return false;
}

bool knobkraft::GenericDataFileLoadCapability::shouldStreamAdvance(std::vector<MidiMessage> const &messages, DataStreamType streamType) const
{
	try {
		int dataType = streamType.asInt();
		std::vector<std::vector<int>> vector;
		for (auto message : messages) {
			vector.push_back(me_->messageToVector(message));
		}
		py::object result = me_->callMethod(kShouldStreamAdvance, dataType, vector);
		return py::cast<bool>(result);
	}
	catch (py::error_already_set &ex) {
		me_->logAdaptationError(kShouldStreamAdvance, ex);
		ex.restore();
	}
	catch (std::exception &ex) {
		me_->logAdaptationError(kShouldStreamAdvance, ex);
	}
	return false;
}

std::vector<std::shared_ptr<midikraft::DataFile>> knobkraft::GenericDataFileLoadCapability::loadData(std::vector<MidiMessage> messages, DataStreamType dataTypeID) const
{
	try {
		int dataType = dataTypeID.asInt();
		std::vector<std::vector<int>> vector;
		for (auto message : messages) {
			vector.push_back(me_->messageToVector(message));
		}
		py::object result = me_->callMethod(kLoadStreamIntoPatches, dataType, vector);
		auto dataOfPatches = result.cast<std::vector<std::vector<int>>>();
		midikraft::TPatchVector patchesFound;
		int i = 0;
		for (auto data : dataOfPatches) {
			std::vector<uint8> byteData = GenericAdaptation::intVectorToByteVector(data);
			auto patch = me_->patchFromPatchData(byteData, MidiProgramNumber::fromZeroBase(i++));
			if (patch) {
				patchesFound.push_back(patch);
			}
			else {
				SimpleLogger::instance()->postMessage((boost::format("Adaptation: Could not create patch from data returned from %s") % kLoadStreamIntoPatches).str());
			}
		}
		return patchesFound;
	}
	catch (py::error_already_set &ex) {
		me_->logAdaptationError(kLoadStreamIntoPatches, ex);
		ex.restore();
	}
	catch (std::exception &ex) {
		me_->logAdaptationError(kLoadStreamIntoPatches, ex);
	}
	return {};
}
