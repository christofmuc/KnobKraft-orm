/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "VerticalPatchButtonList.h"

class SynthBankPanel : public Component
{
public:
	SynthBankPanel();

	virtual void resized() override;

	void setPatches(std::vector<midikraft::PatchHolder> const& patches, PatchButtonInfo info);

private:
	std::unique_ptr<VerticalPatchButtonList> bankList_;
};
