/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchTextBox.h"

#include "Capability.h"
#include "DetailedParametersCapability.h"
#include "LayeredPatchCapability.h"
#include "PatchTextViews.h"

#include <fmt/format.h>

PatchTextBox::PatchTextBox(std::function<void()> forceResize, bool showParams /* = true */) : forceResize_(forceResize), showParams_(showParams)
{
	document_ = std::make_unique<CodeDocument>();
	textBox_ = std::make_unique<CodeEditorComponent>(*document_, nullptr);
	textBox_->setScrollbarThickness(10);
	textBox_->setReadOnly(true);
	textBox_->setLineNumbersShown(false);
	addChildComponent(*textBox_);
	textBox_->setVisible(showParams_);

	addChildComponent(hexBased_);
	hexBased_.setVisible(!showParams_);
	hexBased_.setButtonText("Hex Dump");
	hexBased_.setClickingTogglesState(true);
	hexBased_.onClick = [this]() {
		textBox_->setVisible(hexBased_.getToggleState());
		viewSelector_.setVisible(hexBased_.getToggleState() && viewSelector_.getNumItems() > 1);
		if (forceResize_) forceResize_();
	};

	addChildComponent(viewSelector_);
	viewSelector_.setVisible(showParams_);
	viewSelector_.addItem("Raw hex", 1);
	viewSelector_.setSelectedId(1, dontSendNotification);
	viewSelector_.onChange = [this]() {
		refreshText();
		if (forceResize_) forceResize_();
	};
}

void PatchTextBox::fillTextBox(std::shared_ptr<midikraft::PatchHolder> patch)
{
	auto previousId = viewSelector_.getSelectedId();
	std::string previousName;
	if (previousId >= 3) previousName = customViews_.at(static_cast<size_t>(previousId - 3)).first;
	bool firstPatch = !patch_;
	patch_ = patch;
	customViews_ = patch ? patch_text::viewsFor(*patch) : midikraft::PatchTextViews{};
	viewSelector_.clear(dontSendNotification);
	viewSelector_.addItem("Raw hex", 1);
	bool hasParams = patch && midikraft::Capability::hasCapability<midikraft::DetailedParametersCapability>(patch->patch());
	if (hasParams) viewSelector_.addItem("Parameter values", 2);
	int selectedId = hasParams && (previousId == 2 || (firstPatch && showParams_)) ? 2 : 1;
	for (size_t i = 0; i < customViews_.size(); ++i) {
		auto id = 3 + static_cast<int>(i);
		viewSelector_.addItem(String::fromUTF8(customViews_[i].first.c_str()), id);
		if (previousId >= 3 && customViews_[i].first == previousName) selectedId = id;
	}
	viewSelector_.setSelectedId(selectedId, dontSendNotification);
	viewSelector_.setVisible(showParams_ || (hexBased_.getToggleState() && viewSelector_.getNumItems() > 1));
	hexBased_.setButtonText(viewSelector_.getNumItems() > 1 ? "Patch text" : "Hex Dump");
	refreshText();
	if (forceResize_) forceResize_();
}

void PatchTextBox::refreshText() {
	String text;
	auto selectedId = viewSelector_.getSelectedId();
	if (selectedId == 1) text = makeHexDocument(patch_);
	else if (selectedId == 2) text = makeTextDocument(patch_);
	else text = String::fromUTF8(customViews_.at(static_cast<size_t>(selectedId - 3)).second.c_str());
	// No reflow, trimming, or newline normalization of adaptation-provided text.
	document_->replaceAllContent(text);
}

void PatchTextBox::resized()
{
	auto area = getLocalBounds();

	auto topRow = area.removeFromTop(20);
	if (!showParams_) hexBased_.setBounds(topRow.removeFromLeft(100));
	viewSelector_.setBounds(topRow);

	textBox_->setBounds(area);

	if (viewSelector_.getSelectedId() == 1 && lastLayoutedWidth_.has_value() && *lastLayoutedWidth_ != area.getWidth() && patch_) {
		refreshText();
	}
}

String PatchTextBox::makeHexDocument(std::shared_ptr<midikraft::PatchHolder> patch)
{
	if (!patch || !patch->patch())
		return "No patch active";

	String result;
	std::vector<uint8> binaryData = patch->patch()->data();

	// Find out how many characters we can display
	auto fontUsed = textBox_->getFont();
	auto width = textBox_->getWidth();
	std::string testLine = "0000";
	int testLineLength = 0;
	do {
		testLine += " 00";
		testLineLength += 1;
	} while (TextLayout::getStringWidth(fontUsed, testLine) < width - 22);
	testLineLength = std::max(testLineLength-1, 1);
	lastLayoutedWidth_ = width;

	uint8 *line = binaryData.data();
	int consumed = 0;
	while (consumed < (int)binaryData.size()) {
		uint8 posLSB = consumed & 0xff;
		uint8 posMSB = (consumed >> 8) & 0xff;
		result += String::toHexString(&posMSB, 1, 1) + String::toHexString(&posLSB, 1, 1) + " ";
		int lineLength = std::min(testLineLength, ((int)binaryData.size()) - consumed);
		String hex = String::toHexString(line, lineLength, 1);
		result += hex;
		result += "\n";

		line += lineLength;
		consumed += lineLength;
	}

	return result;
}

float PatchTextBox::desiredHeight() const {
	auto fontUsed = textBox_->getFont();
	auto linesNeeded = (showParams_ || hexBased_.getToggleState()) ? document_->getNumLines() : 0;
	return fontUsed.getHeight() * (linesNeeded + 4);
}

String PatchTextBox::makeTextDocument(std::shared_ptr<midikraft::PatchHolder> patch) {
	if (!patch || !patch->patch())
		return "No patch active";

	auto realPatch = std::dynamic_pointer_cast<midikraft::Patch>(patch->patch());
	if (realPatch) {
		return patchToTextRaw(realPatch, false);
	}
	else {
		return "makeTextDocument not implemented yet";
	}
}

std::string PatchTextBox::patchToTextRaw(std::shared_ptr<midikraft::Patch> patch, bool onlyActive)
{
	std::string result;

	int numLayers = 1;
	auto layers = midikraft::Capability::hasCapability<midikraft::LayeredPatchCapability>(patch);
	if (layers) {
		numLayers = layers->numberOfLayers();
	}

	auto parameterDetails = midikraft::Capability::hasCapability<midikraft::DetailedParametersCapability>(patch);

	if (parameterDetails) {
		for (int layer = 0; layer < numLayers; layer++) {
			if (layers) {
				if (layer > 0) result += "\n";
				result = result + fmt::format("Layer: {}\n", layers->layerName(layer));
			}
			for (auto param : parameterDetails->allParameterDefinitions()) {
				if (layers) {
					auto multiLayerParam = midikraft::Capability::hasCapability<midikraft::SynthMultiLayerParameterCapability>(param);
					jassert(multiLayerParam);
					if (multiLayerParam) {
						multiLayerParam->setSourceLayer(layer);
					}
				}
				auto activeCheck = midikraft::Capability::hasCapability<midikraft::SynthParameterActiveDetectionCapability>(param);
				if (!onlyActive || !activeCheck || !(activeCheck->isActive(patch.get()))) {
					result = result + fmt::format("{}: {}\n",param->description(), param->valueInPatchToText(*patch));
				}
			}
		}
	}
	return result;
}

