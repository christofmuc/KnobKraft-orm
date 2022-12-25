/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "Matrix1000_GlobalSettings.h"

namespace midikraft {

	class Matrix1000_GlobalSettings_DataFile : public DataFile {
	public:
		using DataFile::DataFile;
	};

	midikraft::Matrix1000_GlobalSettings_Loader::Matrix1000_GlobalSettings_Loader(Matrix1000 *matrix1000) : matrix1000_(matrix1000)
	{
	}

	std::vector<juce::MidiMessage> midikraft::Matrix1000_GlobalSettings_Loader::requestDataItem(int itemNo, int dataTypeID)
	{
		ignoreUnused(itemNo, dataTypeID);
		if (matrix1000_) {
			return { matrix1000_->createRequest(Matrix1000::MASTER, 0) };
		}
		return { };
	}

	int midikraft::Matrix1000_GlobalSettings_Loader::numberOfDataItemsPerType(int dataTypeID) const
	{
		ignoreUnused(dataTypeID);
		return 1;
	}

	bool midikraft::Matrix1000_GlobalSettings_Loader::isDataFile(const MidiMessage &message, int dataTypeID) const
	{
		ignoreUnused(dataTypeID);
		return matrix1000_
			&& matrix1000_->isOwnSysex(message)
			&& message.getSysExDataSize() > 3
			&& message.getSysExData()[2] == 0x03 /* Master Parameter Data */
			&& message.getSysExData()[3] == 0x03; /* Matrix 1000 */
	}

	std::vector<std::shared_ptr<midikraft::DataFile>> midikraft::Matrix1000_GlobalSettings_Loader::loadData(std::vector<MidiMessage> messages, int dataTypeID) const
	{
		std::vector<std::shared_ptr<midikraft::DataFile>> result;
		for (auto message : messages) {
			if (isDataFile(message, dataTypeID)) {
				// Extract data from byte 4 onward
				std::vector<uint8> syx(message.getSysExData() + 4, message.getSysExData() + message.getSysExDataSize());
				jassert(syx.size() == 345);
				result.push_back(std::make_shared<Matrix1000_GlobalSettings_DataFile>(Matrix1000::DF_MATRIX1000_SETTINGS, syx));
			}
		}
		return result;
	}

	std::vector<midikraft::DataFileLoadCapability::DataFileDescription> midikraft::Matrix1000_GlobalSettings_Loader::dataTypeNames() const
	{
		// No user visible data types
		return {};
	}

}
