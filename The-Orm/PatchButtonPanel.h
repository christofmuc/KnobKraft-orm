/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PatchHolder.h"
#include "PatchButtonGrid.h"

#include "MidiController.h"
#include "Synth.h"

class PatchButtonPanel : public Component,
	private Button::Listener
{
public:
	PatchButtonPanel(std::function<void(midikraft::PatchHolder &)> handler);
	virtual ~PatchButtonPanel();

	void setPatches(std::vector<midikraft::PatchHolder> const &patches);
	void refresh(bool keepActive);

	void resized() override;

	void buttonClicked(Button* button) override;

	void buttonClicked(int buttonIndex);

private:
	bool isMacroMessage(const MidiMessage& message);
	void executeMacro(const MidiMessage& message);

	midikraft::MidiController::HandlerHandle callback_  = midikraft::MidiController::makeOneHandle();
	std::vector<midikraft::PatchHolder> patches_;
	std::unique_ptr<PatchButtonGrid> patchButtons_;
	std::function<void(midikraft::PatchHolder &)> handler_;
	int indexOfActive_;

	TextButton pageUp_, pageDown_;
	Label pageNumbers_;
	int pageBase_;
	int pageNumber_;
	int pageSize_;
};

