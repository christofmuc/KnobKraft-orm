/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchHolderButton.h"

#include "ColourHelpers.h"
#include "LayeredPatchCapability.h"

#include "UIModel.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <tuple>

PatchColourMode PatchHolderButton::patchColourMode_ = PatchColourMode::Primary;

class PatchHolderButton::StripedLookAndFeel : public LookAndFeel_V4 {
public:
	explicit StripedLookAndFeel(PatchHolderButton& owner) : owner_(owner)
	{
		if (auto* inherited = dynamic_cast<LookAndFeel_V4*>(&owner.getLookAndFeel())) {
			setColourScheme(inherited->getCurrentColourScheme());
		}
	}

	void drawButtonBackground(Graphics& g, Button& button, Colour const& backgroundColour, bool shouldDrawButtonAsHighlighted,
		bool shouldDrawButtonAsDown) override
	{
		const bool drawStripes = !button.getToggleState()
			&& PatchHolderButton::patchColourMode() == PatchColourMode::Striped
			&& owner_.patchColours_.size() > 1;

		if (!drawStripes) {
			LookAndFeel_V4::drawButtonBackground(g, button, backgroundColour, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
			return;
		}

		// Let JUCE draw the normal shape and border using the stable primary colour,
		// then clip the secondary stripes into that shape.
		LookAndFeel_V4::drawButtonBackground(g, button, owner_.patchColours_.front(), false, false);

		Graphics::ScopedSaveState savedState(g);
		auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
		Path clipPath;
		clipPath.addRoundedRectangle(bounds, 5.0f);
		g.reduceClipRegion(clipPath);

		constexpr float stripeWidth = 10.0f;
		constexpr float stripeAngle = MathConstants<float>::pi / 6.0f;
		const float diagonal = std::sqrt(bounds.getWidth() * bounds.getWidth() + bounds.getHeight() * bounds.getHeight());
		const auto centre = bounds.getCentre();
		const float firstStripe = centre.x - diagonal;
		const float lastStripe = centre.x + diagonal;
		int band = 0;
		int secondaryColour = 0;

		for (float x = firstStripe; x < lastStripe; x += stripeWidth, ++band) {
			// Leave every other band in the primary colour. For three or more
			// categories, rotate the remaining colours through the other bands.
			if ((band % 2) == 0) {
				continue;
			}

			Path stripe;
			stripe.addRectangle(x, centre.y - diagonal, stripeWidth, diagonal * 2.0f);
			stripe.applyTransform(AffineTransform::rotation(stripeAngle, centre.x, centre.y));
			auto colour = owner_.patchColours_[1 + (secondaryColour++ % (owner_.patchColours_.size() - 1))];
			g.setColour(colour.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));
			g.fillPath(stripe);
		}

		// Apply interaction feedback uniformly instead of only to the primary bands.
		if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown) {
			g.setColour(Colours::white.withAlpha(shouldDrawButtonAsDown ? 0.16f : 0.08f));
			g.fillRoundedRectangle(bounds, 5.0f);
		}
	}

private:
	PatchHolderButton& owner_;
};

std::vector<Colour> PatchHolderButton::sortedColoursForCategories(std::set<midikraft::Category> const& categories,
	Component* componentForDefaultBackground)
{
	std::vector<midikraft::Category> sortedCategories(categories.cbegin(), categories.cend());
	std::sort(sortedCategories.begin(), sortedCategories.end(), [](auto const& left, auto const& right) {
		return std::tie(left.def()->sort_order, left.def()->id) < std::tie(right.def()->sort_order, right.def()->id);
	});

	std::vector<Colour> colours;
	colours.reserve(std::max<size_t>(1, sortedCategories.size()));
	for (auto const& category : sortedCategories) {
		colours.push_back(category.color());
	}
	if (colours.empty()) {
		colours.push_back(ColourHelpers::getUIColour(componentForDefaultBackground, LookAndFeel_V4::ColourScheme::widgetBackground));
	}
	return colours;
}

Colour PatchHolderButton::buttonColourForPatch(midikraft::PatchHolder &patch, Component *componentForDefaultBackground) {
	return sortedColoursForCategories(patch.categories(), componentForDefaultBackground).front();
}

PatchColourMode PatchHolderButton::patchColourMode()
{
	return patchColourMode_;
}

void PatchHolderButton::setPatchColourMode(PatchColourMode mode)
{
	if (patchColourMode_ != mode) {
		patchColourMode_ = mode;
		UIModel::instance()->patchColourModeChanged.sendChangeMessage();
	}
}

PatchHolderButton::PatchHolderButton(int id, bool isToggle, std::function<void(int)> clickHandler) : PatchButtonWithDropTarget(id, isToggle, clickHandler)
	, isDirty_(false)
{
	stripedLookAndFeel_ = std::make_unique<StripedLookAndFeel>(*this);
	for (int child = 0; child < getNumChildComponents(); ++child) {
		if (auto* button = dynamic_cast<Button*>(getChildComponent(child))) {
			patchButton_ = button;
			patchButton_->setLookAndFeel(stripedLookAndFeel_.get());
			break;
		}
	}
	UIModel::instance()->currentPatch_.addChangeListener(this);
	UIModel::instance()->categoriesChanged.addChangeListener(this);
	UIModel::instance()->patchColourModeChanged.addChangeListener(this);
}

PatchHolderButton::~PatchHolderButton()
{
	if (patchButton_) {
		patchButton_->setLookAndFeel(nullptr);
	}
	UIModel::instance()->patchColourModeChanged.removeChangeListener(this);
	UIModel::instance()->categoriesChanged.removeChangeListener(this);
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
		setFavorite(holder->isFavorite());
		setHidden(holder->isHidden());
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
			setButtonData(holder->name());
			break;
		}
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

		patchCategories_ = holder->categories();
		refreshPatchColours();
		setFavorite(holder->isFavorite());
		setHidden(holder->isHidden());
	}
	else {
		setButtonData("");
		setSubtitle("");
		patchCategories_.clear();
		refreshPatchColours();
		setFavorite(false);
		setHidden(false);
		md5_.reset();
	}
	refreshActiveState();
}

void PatchHolderButton::refreshPatchColours()
{
	patchColours_ = sortedColoursForCategories(patchCategories_, this);
	setPatchColour(TextButton::ColourIds::buttonColourId, patchColours_.front());
	if (patchButton_) {
		patchButton_->repaint();
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
	else if (source == &UIModel::instance()->categoriesChanged) {
		// Category definitions are shared by the patch holders, so re-reading the
		// colours here picks up user edits without reloading the page first.
		refreshPatchColours();
	}
	else if (source == &UIModel::instance()->patchColourModeChanged) {
		if (patchButton_) {
			patchButton_->repaint();
		}
	}
}
