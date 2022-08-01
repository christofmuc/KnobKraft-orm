/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchHolderButton.h"

#include "ColourHelpers.h"
#include "LayeredPatchCapability.h"

#include "UIModel.h"

Colour PatchHolderButton::buttonColourForPatch(midikraft::PatchHolder &patch, Component *componentForDefaultBackground) {
	Colour color = ColourHelpers::getUIColour(componentForDefaultBackground, LookAndFeel_V4::ColourScheme::widgetBackground);
	auto cats = patch.categories();
	if (!cats.empty()) {
		// Random in case the patch has multiple categories
		color = cats.cbegin()->color();
	}
	return color;
}

PatchHolderButton::PatchHolderButton(int id, bool isToggle, std::function<void(int)> clickHandler) : PatchButtonWithDropTarget(id, isToggle, clickHandler)
	, isDirty_(false)
{
}

void PatchHolderButton::setDirty(bool isDirty)
{
	isDirty_ = isDirty;
	setGlow(false);
}


void PatchHolderButton::setGlow(bool shouldGlow)
{
	if (shouldGlow) {
		// The drop indication colour overrides the isDirty background
		glow.setGlowProperties(4.0, Colours::gold.withAlpha(0.5f));
		setComponentEffect(&glow);
	}
	else {
		if (isDirty_) {
			glow.setGlowProperties(4.0, Colours::darkred);
			setComponentEffect(&glow);
		}
		else {
			// Neither
			setComponentEffect(nullptr);
		}
	}
	repaint();
}

void PatchHolderButton::itemDragEnter(const SourceDetails& dragSourceDetails)
{
	setGlow(acceptsItem(dragSourceDetails.description));
}

void PatchHolderButton::itemDragExit(const SourceDetails& dragSourceDetails)
{
	ignoreUnused(dragSourceDetails);
	setGlow(false);
}

void PatchHolderButton::setPatchHolder(midikraft::PatchHolder *holder, bool active, PatchButtonInfo info)
{
	setActive(active);
	Colour color = ColourHelpers::getUIColour(this, LookAndFeel_V4::ColourScheme::widgetBackground);
	if (holder) {
		auto number = holder->synth()->friendlyProgramName(holder->patchNumber());
		auto dragInfo = holder->createDragInfoString();
		switch (static_cast<PatchButtonInfo>(static_cast<int>(info) & static_cast<int>(PatchButtonInfo::CenterMask))) {
		case PatchButtonInfo::CenterLayers: {
			auto layers = midikraft::Capability::hasCapability<midikraft::LayeredPatchCapability>(holder->patch());
			if (layers) {
				if (layers->layerName(0) != layers->layerName(1)) {
					setButtonData(layers->layerName(0), layers->layerName(1), dragInfo);
				}
				else {
					setButtonData(layers->layerName(0), dragInfo);
				}
				break;
			}
		}
		// FallThrough
		case PatchButtonInfo::CenterName:
			setButtonData(holder->name(), dragInfo);
			break;
		case PatchButtonInfo::CenterNumber:
			setButtonData(number, dragInfo);
			break;
		default:
			// Please make sure your enum bit flags work the way we expect
			jassertfalse;
			setButtonData(number, dragInfo);
		}

		switch (static_cast<PatchButtonInfo>(static_cast<int>(info) & static_cast<int>(PatchButtonInfo::SubtitleMask))) {
		case PatchButtonInfo::NoneMasked:
			setSubtitle("");
			break;
		case PatchButtonInfo::SubtitleNumber:
			setSubtitle(number);
			break;
		case PatchButtonInfo::SubtitleSynth:
			setSubtitle(holder->synth() ? holder->synth()->getName() : "");
			break;
		default:
			jassertfalse;
			// Your bit masks don't work as you expect
			setSubtitle("");
		}
		setColour(TextButton::ColourIds::buttonColourId, buttonColourForPatch(*holder, this));
		setFavorite(holder->isFavorite());
		setHidden(holder->isHidden());
	}
	else {
		setButtonData("", "");
		setSubtitle("");
		setColour(TextButton::ColourIds::buttonColourId, color);
		setFavorite(false);
		setHidden(false);
	}
}

PatchButtonInfo PatchHolderButton::getCurrentInfoForSynth(std::string const& synthname) {
	return static_cast<PatchButtonInfo>(
		static_cast<int>(
			UIModel::getSynthSpecificPropertyAsValue(synthname, PROPERTY_BUTTON_INFO_TYPE, static_cast<int>(PatchButtonInfo::DefaultDisplay)).getValue()
		)
	);
}

void PatchHolderButton::setCurrentInfoForSynth(std::string const& synthname, PatchButtonInfo newValue) {
	auto synth = UIModel::ensureSynthSpecificPropertyExists(synthname, PROPERTY_BUTTON_INFO_TYPE, static_cast<int>(PatchButtonInfo::DefaultDisplay));
	synth.setProperty(PROPERTY_BUTTON_INFO_TYPE, static_cast<int>(newValue), nullptr);
}
