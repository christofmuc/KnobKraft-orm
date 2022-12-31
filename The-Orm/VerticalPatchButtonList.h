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
	typedef std::function<void(MidiProgramNumber, std::string const&, std::string const&)> TListDropHandler;

	VerticalPatchButtonList(std::function<void(MidiProgramNumber, std::string)> dropHandler, TListDropHandler listDropHandler, std::function<int(std::string const&, std::string const&)> listResolver);

	std::function<void(midikraft::PatchHolder&)> onPatchClicked;

	virtual void resized() override;

	void setPatches(std::shared_ptr<midikraft::SynthBank> bank, PatchButtonInfo info);
	void refreshContent();

private:
	std::function<void(MidiProgramNumber, std::string)> dropHandler_;
	TListDropHandler listDropHandler_;
	ListBox list_;
	std::function<int(std::string const&, std::string const&)> listResolver_;
};
