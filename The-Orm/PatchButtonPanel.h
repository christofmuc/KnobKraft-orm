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
	typedef std::function<void(int, int, std::function<void(std::vector<midikraft::PatchHolder>)>)> TPageLoader;

	PatchButtonPanel(std::function<void(midikraft::PatchHolder &)> handler);
	virtual ~PatchButtonPanel();

	void setPatchLoader(TPageLoader pageGetter);
	void setTotalCount(int totalCount);
	void setPatches(std::vector<midikraft::PatchHolder> const &patches);
	void refresh(bool async);

	void resized() override;

	void buttonClicked(Button* button) override;

	void buttonClicked(int buttonIndex);

private:
	bool isMacroMessage(const MidiMessage& message);
	void executeMacro(const MidiMessage& message);
	int indexOfActive() const;

	midikraft::MidiController::HandlerHandle callback_  = midikraft::MidiController::makeOneHandle();
	std::vector<midikraft::PatchHolder> patches_;
	std::unique_ptr<PatchButtonGrid> patchButtons_;
	std::function<void(midikraft::PatchHolder &)> handler_;
	TPageLoader pageLoader_;

	std::string activePatchMd5_;

	TextButton pageUp_, pageDown_;
	Label pageNumbers_;
	int pageBase_;
	int pageNumber_;
	int pageSize_;
	int totalSize_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchButtonPanel)
};

