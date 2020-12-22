/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/


#include "OB6Patch.h"

#include "PatchNumber.h"

#include <boost/format.hpp>

namespace midikraft {

	OB6Patch::OB6Patch(int dataTypeID, Synth::PatchData const &patchData, MidiProgramNumber programNo) : Patch(dataTypeID, patchData), place_(programNo)
	{
	}

	std::string OB6Patch::name() const
	{
		// The OB6 has a 20 character patch name storage
		std::string result;
		for (int i = 107; i < 127; i++) {
			result.push_back(data()[i]);
		}
		return result;
	}

	void OB6Patch::setName(std::string const &name)
	{
		int baseIndex = 107;
		for (int i = 0; i < 20; i++) {
			if (i < (int)name.size()) {
				setAt(baseIndex + i, name[i]);
			}
			else {
				// Fill the 20 characters with space
				setAt(baseIndex + i, ' ');
			}
		}
	}

	bool OB6Patch::isDefaultName() const
	{
		return name() == "Basic Program";
	}

	MidiProgramNumber OB6Patch::patchNumber() const
	{
		return place_;
	}

}
