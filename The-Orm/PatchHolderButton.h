/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "PatchButton.h"
#include "PatchHolder.h"

#include <vector>

enum class PatchButtonInfo {
	NoneMasked = 0b0,
	CenterName = 0b001,
	CenterLayers = 0b011,
	CenterNumber = 0b111,
	CenterMask = 0b111,
	SubtitleAuthor = 0b001000,
	SubtitleNumber = 0b011000,
	SubtitleSynth =  0b111000,
	SubtitleMask =   0b111000,
	DefaultDisplay = CenterLayers | SubtitleNumber,
	ProgramDisplay = CenterNumber, 
	NameDisplay = CenterName | SubtitleNumber,
	NameAuthorDisplay = CenterName | SubtitleAuthor,
	LayerDisplay = DefaultDisplay,
};

enum class PatchColourMode {
	Primary,
	Striped,
};

class PatchHolderButton : public PatchButtonWithDropTarget, private juce::ChangeListener {
public:
	PatchHolderButton(int id, bool isToggle, std::function<void(int)> clickHandler);
	virtual ~PatchHolderButton();

	void setDirty(bool isDirty);
	void setGlow(bool glow);

	// Need to visualize the drag
	virtual void itemDragEnter(const SourceDetails& dragSourceDetails) override;
	virtual void itemDragExit(const SourceDetails& dragSourceDetails) override;

	void setPatchHolder(midikraft::PatchHolder *holder, PatchButtonInfo info);

	static Colour buttonColourForPatch(midikraft::PatchHolder &patch, Component *componentForDefaultBackground);
	static PatchColourMode patchColourMode();
	static void setPatchColourMode(PatchColourMode mode);
	static PatchButtonInfo getCurrentInfoForSynth(std::string const& synthname);
	static void setCurrentInfoForSynth(std::string const& synthname, PatchButtonInfo newValue);

private:
	class StripedLookAndFeel;

	static std::vector<Colour> sortedColoursForCategories(std::set<midikraft::Category> const& categories, Component* componentForDefaultBackground);
	void refreshPatchColours();
	void refreshActiveState();
	virtual void changeListenerCallback(ChangeBroadcaster* source) override;

	static PatchColourMode patchColourMode_;
	std::unique_ptr<StripedLookAndFeel> stripedLookAndFeel_;
	juce::Button* patchButton_ = nullptr;
	std::set<midikraft::Category> patchCategories_;
	std::vector<Colour> patchColours_;
	std::optional<std::string> md5_;
	bool isDirty_;
	GlowEffect glow;
	juce::Value number_;
};

