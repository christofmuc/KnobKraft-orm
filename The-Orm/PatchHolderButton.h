/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "PatchButton.h"
#include "PatchHolder.h"

enum class PatchButtonInfo {
	NoneMasked = 0b0,
	CenterName = 0b001,
	CenterLayers = 0b011,
	CenterNumber = 0b111,
	CenterMask = 0b111,
	SubtitleNumber = 0b11000,
	SubtitleSynth = 0b111000,
	SubtitleMask = 0b111000,
	DefaultDisplay = CenterLayers | SubtitleNumber,
	ProgramDisplay = CenterNumber, 
	NameDisplay = CenterName | SubtitleNumber,
	LayerDisplay = DefaultDisplay
};

class PatchHolderButton : public PatchButtonWithDropTarget {
public:
	PatchHolderButton(int id, bool isToggle, std::function<void(int)> clickHandler);

	void setDirty(bool isDirty);
	void setGlow(bool glow);

	// Need to visualize the drag
	virtual void itemDragEnter(const SourceDetails& dragSourceDetails) override;
	virtual void itemDragExit(const SourceDetails& dragSourceDetails) override;

	void setPatchHolder(midikraft::PatchHolder *holder, bool active, PatchButtonInfo info);

	static Colour buttonColourForPatch(midikraft::PatchHolder &patch, Component *componentForDefaultBackground);
	static PatchButtonInfo getCurrentInfoForSynth(std::string const& synthname);
	static void setCurrentInfoForSynth(std::string const& synthname, PatchButtonInfo newValue);

private:
	juce::ValueTree createValueTwin(midikraft::PatchHolder*);
	void rebindButton(ValueTree patchValues);

	static juce::ValueTree emptyButtonValues_;

	bool isDirty_;
	GlowEffect glow;
	juce::Value number_;
};

