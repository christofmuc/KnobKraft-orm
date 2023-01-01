/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "DataFileLoadCapability.h"

#include "Matrix1000.h"

namespace midikraft {

	class Matrix1000_GlobalSettings_Loader : public DataFileLoadCapability {
	public:
		Matrix1000_GlobalSettings_Loader(Matrix1000 *matrix1000);
        virtual ~Matrix1000_GlobalSettings_Loader() = default;

		virtual std::vector<MidiMessage> requestDataItem(int itemNo, int dataTypeID) override;
		virtual int numberOfDataItemsPerType(int dataTypeID) const override;
		virtual bool isDataFile(const MidiMessage &message, int dataTypeID) const override;
		virtual std::vector<std::shared_ptr<DataFile>> loadData(std::vector<MidiMessage> messages, int dataTypeID) const override;
		virtual std::vector<DataFileDescription> dataTypeNames() const  override;

	private:
		Matrix1000 *matrix1000_;
	};

}

