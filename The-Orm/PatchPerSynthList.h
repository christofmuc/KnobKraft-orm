/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PatchHolderButton.h"

class PatchPerSynthList : public Component, private ChangeListener {
public:
	PatchPerSynthList();
	virtual ~PatchPerSynthList();

	virtual void resized() override;

	void setPatches(std::vector<midikraft::PatchHolder> const &patches);

private:
	void changeListenerCallback(ChangeBroadcaster* source) override;

	std::vector<std::shared_ptr<PatchHolderButton>> patchButtons_;
	std::map<std::string, std::shared_ptr<PatchHolderButton>> buttonForSynth_;
};

