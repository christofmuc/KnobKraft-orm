/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "DataFileLoadCapability.h"

#include "GenericAdaptation.h"

namespace knobkraft {

	class GenericDataFileLoadCapability : public midikraft::DataFileLoadCapability {
	public:
		GenericDataFileLoadCapability(GenericAdaptation *me) : me_(me) {}


		std::vector<DataFileDescription> dataTypeNames() const override;
		bool isDataFile(const MidiMessage &message, midikraft::DataFileType dataTypeID) const override;

		std::vector<DataFileImportDescription> dataFileImportChoices() const override;
		std::vector<MidiMessage> requestDataItem(int itemNo, midikraft::DataStreamType streamType) override;
		int numberOfMidiMessagesPerStreamType(midikraft::DataStreamType dataTypeID) const override;
		bool isPartOfDataFileStream(const MidiMessage &message, midikraft::DataStreamType dataTypeID) const override;
		bool isStreamComplete(std::vector<MidiMessage> const &messages, midikraft::DataStreamType streamType) const override;
		bool shouldStreamAdvance(std::vector<MidiMessage> const &messages, midikraft::DataStreamType streamType) const override;
		std::vector<std::shared_ptr<midikraft::DataFile>> loadData(std::vector<MidiMessage> messages, midikraft::DataStreamType dataTypeID) const override;

	private:
		GenericAdaptation *me_;
	};

}
