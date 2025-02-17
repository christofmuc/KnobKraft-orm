/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchHolderButton.h"

#include "ColourHelpers.h"
#include "LayeredPatchCapability.h"

#include "UIModel.h"

#include <fmt/format.h>

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
	UIModel::instance()->currentPatch_.addChangeListener(this);
}

PatchHolderButton::~PatchHolderButton()
{
	UIModel::instance()->currentPatch_.removeChangeListener(this);
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
		setBufferedToImage(true);
	}
	else {
		if (isDirty_) {
			glow.setGlowProperties(4.0, Colours::darkred);
			setComponentEffect(&glow);
			setBufferedToImage(true);
		}
		else {
			// Neither
			setComponentEffect(nullptr);
			setBufferedToImage(false);
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

void PatchHolderButton::setPatchHolder(midikraft::PatchHolder *holder, PatchButtonInfo info)
{
	if (holder) {
		auto number = juce::String(holder->synth()->friendlyProgramAndBankName(holder->bankNumber(), holder->patchNumber()));
		auto dragInfo = holder->createDragInfoString();
		setButtonDragInfo(dragInfo);
		md5_ = holder->md5();
		switch (static_cast<PatchButtonInfo>(static_cast<int>(info) & static_cast<int>(PatchButtonInfo::CenterMask))) {
		case PatchButtonInfo::CenterLayers: {
			auto layers = midikraft::Capability::hasCapability<midikraft::LayeredPatchCapability>(holder->patch());
			if (layers) {
				if (layers->layerName(0) != layers->layerName(1)) {
					String multiLineTitle = String(layers->layerName(0)) + "\n" + String(layers->layerName(1));
					setButtonData(multiLineTitle);
				}
				else {
					setButtonData(layers->layerName(0));
				}
				break;
			}
		}
		// FallThrough
		case PatchButtonInfo::CenterName:
			setButtonData(holder->name());
			break;
		case PatchButtonInfo::CenterNumber:
			setButtonData(number);
			break;
		default:
			// Please make sure your enum bit flags work the way we expect
			jassertfalse;
			setButtonData(number);
		}

		switch (static_cast<PatchButtonInfo>(static_cast<int>(info) & static_cast<int>(PatchButtonInfo::SubtitleMask))) {
		case PatchButtonInfo::NoneMasked:
			setSubtitle("");
			break;
		case PatchButtonInfo::SubtitleAuthor:
			setSubtitle(holder->author());
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

		setPatchColour(TextButton::ColourIds::buttonColourId, buttonColourForPatch(*holder, this));
		setFavorite(holder->isFavorite());
		setHidden(holder->isHidden());
	}
	else {
		Colour color = ColourHelpers::getUIColour(this, LookAndFeel_V4::ColourScheme::widgetBackground);
		setButtonData("");
		setSubtitle("");
		setPatchColour(TextButton::ColourIds::buttonColourId, color);
		setFavorite(false);
		setHidden(false);
		md5_.reset();
	}
	refreshActiveState();
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

void PatchHolderButton::refreshActiveState()
{
	if (md5_.has_value() && UIModel::currentPatch().patch() != nullptr) {
		setActive(*md5_ == UIModel::currentPatch().md5());
	}
	else {
		setActive(false);
	}
}

void PatchHolderButton::changeListenerCallback(ChangeBroadcaster* source) {
	if (source == &UIModel::instance()->currentPatch_) {
		refreshActiveState();
	}
}