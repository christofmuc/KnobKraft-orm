/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchTextBox.h"

#include "Capability.h"
#include "DetailedParametersCapability.h"
#include "LayeredPatchCapability.h"

#include <fmt/format.h>

PatchTextBox::PatchTextBox(std::function<void()> forceResize, bool showParams /* = true */) : forceResize_(forceResize), showParams_(showParams), mode_(showParams ? DisplayMode::PARAMS : DisplayMode::HEX)
{
	document_ = std::make_unique<CodeDocument>();
	textBox_ = std::make_unique<CodeEditorComponent>(*document_, nullptr);
	textBox_->setScrollbarThickness(10);

	textBox_->setReadOnly(true);
	textBox_->setLineNumbersShown(false);
	addChildComponent(*textBox_);

	addAndMakeVisible(hexBased_);
	hexBased_.setButtonText(showParams_ ? "Show hex values" : "Hex Dump");
	hexBased_.setClickingTogglesState(true);
	
	if (showParams_) {
		textBox_->setVisible(true);
		hexBased_.setRadioGroupId(3, dontSendNotification);
		hexBased_.onClick = [this]() {
			mode_ = DisplayMode::HEX;
			refreshText();
		};

		addAndMakeVisible(textBased_);
		textBased_.setButtonText("Show parameter values");
		textBased_.setToggleState(true, dontSendNotification);
		textBased_.setRadioGroupId(3, dontSendNotification);
		textBased_.setClickingTogglesState(true);
		textBased_.setVisible(false);
		textBased_.onClick = [this]() {
			mode_ = DisplayMode::PARAMS;
			refreshText();
		};

		hexBased_.setToggleState(true, dontSendNotification);
	}
	else {
		hexBased_.setToggleState(false, dontSendNotification);
		hexBased_.onClick = [this]() {
			textBox_->setVisible(hexBased_.getToggleState());
			if (forceResize_) {
				forceResize_();
			}
		};
	}
}

void PatchTextBox::fillTextBox(std::shared_ptr<midikraft::PatchHolder> patch)
{
	patch_ = patch;

	if (patch) {
		// If there is detailed parameter information, also show the second option
		if (showParams_) {
			auto parameterDetails = midikraft::Capability::hasCapability<midikraft::DetailedParametersCapability>(patch->patch());
			if (parameterDetails) {
				textBased_.setVisible(true);
			}
			else {
				mode_ = DisplayMode::HEX;
				textBased_.setVisible(false);
				hexBased_.setToggleState(true, dontSendNotification);
			}
		}
		refreshText();
	}
}

void PatchTextBox::refreshText() {
	String text;
	switch (mode_) {
	case DisplayMode::HEX:
		text = makeHexDocument(patch_);
		break;
	case DisplayMode::PARAMS:
		text = makeTextDocument(patch_);
		break;
	}
	document_->replaceAllContent(text);
}

void PatchTextBox::resized()
{
	auto area = getLocalBounds();

	auto topRow = area.removeFromTop(20);
	hexBased_.setBounds(topRow.removeFromLeft(100));
	textBased_.setBounds(topRow.removeFromLeft(100));

	textBox_->setBounds(area);

	if (lastLayoutedWidth_.has_value() && *lastLayoutedWidth_ != area.getWidth() && patch_) {
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

