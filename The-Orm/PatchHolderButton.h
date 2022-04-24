/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "PatchButton.h"
#include "PatchHolder.h"

enum class PatchButtonInfo {
	CenterName = 0b001,
	CenterLayers = 0b011,
	CenterNumber = 0b111,
	CenterMask = 0b111,
	SubtitleNone = 0b1000,
	SubtitleNumber = 0b11000,
	SubtitleSynth = 0b111000,
	SubtitleMask = 0b111000,
	DefaultDisplay = CenterLayers | SubtitleNumber,
	ProgramDisplay = CenterNumber, 
	NameDisplay = CenterName | SubtitleNumber,
	LayerDisplay = DefaultDisplay
};

class PatchHolderButton : public PatchButton {
public:
	using PatchButton::PatchButton;
	
	void setPatchHolder(midikraft::PatchHolder *holder, bool active, PatchButtonInfo info);

	static Colour buttonColourForPatch(midikraft::PatchHolder &patch, Component *componentForDefaultBackground);
	static PatchButtonInfo getCurrentInfoForSynth(std::string const& synthname);
	static void setCurrentInfoForSynth(std::string const& synthname, PatchButtonInfo newValue);
};

