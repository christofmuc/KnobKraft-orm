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
	Data::ensureEphemeralPropertyExists(EPROPERTY_PATCH_CACHE, "");

	if (emptyButtonValues_.getNumProperties() == 0) {
		emptyButtonValues_.setProperty(EPROPERTY_PATCH_TITLE, String(""), nullptr);
		emptyButtonValues_.setProperty(EPROPERTY_PATCH_SUBTITLE, String(""), nullptr);
		emptyButtonValues_.setProperty(EPROPERTY_PATCH_FAVORITE, false, nullptr);
		emptyButtonValues_.setProperty(EPROPERTY_PATCH_HIDDEN, false, nullptr);
		Colour color = ColourHelpers::getUIColour(this, LookAndFeel_V4::ColourScheme::widgetBackground);
		emptyButtonValues_.setProperty(EPROPERTY_PATCH_COLOR, color.toString(), nullptr);
	}
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

juce::ValueTree PatchHolderButton::createValueTwin(midikraft::PatchHolder* patch) {
	//TODO - this might cause memory growth if we never clear the cache. Can we somehow count how many buttons reference these ephemeral values?

	auto cache = Data::instance().getEphemeral().getOrCreateChildWithName(EPROPERTY_PATCH_CACHE, nullptr);
	return cache.getOrCreateChildWithName(juce::Identifier(patch->md5()), nullptr);
}

void PatchHolderButton::rebindButton(ValueTree patchValue) {
	bindButtonData(patchValue.getPropertyAsValue(EPROPERTY_PATCH_TITLE, nullptr));
	bindSubtitle(patchValue.getPropertyAsValue(EPROPERTY_PATCH_SUBTITLE, nullptr));
	bindColour(TextButton::ColourIds::buttonColourId, patchValue.getPropertyAsValue(EPROPERTY_PATCH_COLOR, nullptr));
	bindFavorite(patchValue.getPropertyAsValue(EPROPERTY_PATCH_FAVORITE, nullptr));
	bindHidden(patchValue.getPropertyAsValue(EPROPERTY_PATCH_HIDDEN, nullptr));
}

void PatchHolderButton::setPatchHolder(midikraft::PatchHolder *holder, bool active, PatchButtonInfo info)
{
	setActive(active);
	
	if (holder) {
		auto patchValue = createValueTwin(holder);
		auto number = holder->synth()->friendlyProgramAndBankName(holder->bankNumber(), holder->patchNumber());
		
		auto dragInfo = holder->createDragInfoString();
		setButtonDragInfo(dragInfo);

		switch (static_cast<PatchButtonInfo>(static_cast<int>(info) & static_cast<int>(PatchButtonInfo::CenterMask))) {
		case PatchButtonInfo::CenterLayers: {
			auto layers = midikraft::Capability::hasCapability<midikraft::LayeredPatchCapability>(holder->patch());
			if (layers) {
				if (layers->layerName(0) != layers->layerName(1)) {
					String multiLineTitle = String(layers->layerName(0)).trim() + "\n" + String(layers->layerName(1)).trim();
					patchValue.setProperty(EPROPERTY_PATCH_TITLE, multiLineTitle, nullptr);
				}
				else {
					patchValue.setProperty(EPROPERTY_PATCH_TITLE, String(layers->layerName(0)), nullptr);
				}
				break;
			}
		}
		// FallThrough
		case PatchButtonInfo::CenterName:
			patchValue.setProperty(EPROPERTY_PATCH_TITLE, String(holder->name()), nullptr);
			break;
		case PatchButtonInfo::CenterNumber:
			patchValue.setProperty(EPROPERTY_PATCH_TITLE, String(number), nullptr);
			break;
		default:
			// Please make sure your enum bit flags work the way we expect
			jassertfalse;
			patchValue.setProperty(EPROPERTY_PATCH_TITLE, String(number), nullptr);
		}

		switch (static_cast<PatchButtonInfo>(static_cast<int>(info) & static_cast<int>(PatchButtonInfo::SubtitleMask))) {
		case PatchButtonInfo::NoneMasked:
			patchValue.setProperty(EPROPERTY_PATCH_SUBTITLE, String(""), nullptr);
			break;
		case PatchButtonInfo::SubtitleNumber:
			patchValue.setProperty(EPROPERTY_PATCH_SUBTITLE, String(number), nullptr);
			break;
		case PatchButtonInfo::SubtitleSynth:
			patchValue.setProperty(EPROPERTY_PATCH_SUBTITLE, String(holder->synth() ? holder->synth()->getName() : ""), nullptr);
			break;
		default:
			jassertfalse;
			// Your bit masks don't work as you expect
			patchValue.setProperty(EPROPERTY_PATCH_SUBTITLE, String(""), nullptr);
		}

		patchValue.setProperty(EPROPERTY_PATCH_FAVORITE, holder->isFavorite(), nullptr);
		patchValue.setProperty(EPROPERTY_PATCH_HIDDEN, holder->isHidden(), nullptr);
		patchValue.setProperty(EPROPERTY_PATCH_COLOR, buttonColourForPatch(*holder, this).toString(), nullptr);
		rebindButton(patchValue);
	}
	else {
		rebindButton(emptyButtonValues_);
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

juce::ValueTree PatchHolderButton::emptyButtonValues_("emptyPatchButton");
