/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once


#include "PatchHolder.h"
#include "PatchHolderButton.h"
#include "SynthBank.h"

class VerticalPatchButtonList : public Component {
public:
	VerticalPatchButtonList(std::function<void(MidiProgramNumber, std::string)> dropHandler);

	virtual void resized() override;

	void setPatches(std::shared_ptr<midikraft::SynthBank> bank, PatchButtonInfo info);

private:
	std::function<void(MidiProgramNumber, std::string)> dropHandler_;
	ListBox list_;
};
