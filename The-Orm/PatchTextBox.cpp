/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchTextBox.h"

#include "Capability.h"
#include "DetailedParametersCapability.h"
#include "LayeredPatchCapability.h"

#include <boost/format.hpp>

PatchTextBox::PatchTextBox() : mode_(DisplayMode::PARAMS)
{
	document_ = std::make_unique<CodeDocument>();
	textBox_ = std::make_unique<CodeEditorComponent>(*document_, nullptr);
	textBox_->setScrollbarThickness(10);

	textBox_->setReadOnly(true);
	textBox_->setLineNumbersShown(false);
	addAndMakeVisible(*textBox_);

	addAndMakeVisible(hexBased_);
	hexBased_.setButtonText("Show hex values");
	hexBased_.setClickingTogglesState(true);
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

void PatchTextBox::fillTextBox(std::shared_ptr<midikraft::PatchHolder> patch)
{
	patch_ = patch;

	// If there is detailed parameter information, also show the second option
	auto parameterDetails = midikraft::Capability::hasCapability<midikraft::DetailedParametersCapability>(patch->patch());
	if (parameterDetails) {
		textBased_.setVisible(true);
	}
	else {
		mode_ = DisplayMode::HEX;
		textBased_.setVisible(false);
		hexBased_.setToggleState(true, dontSendNotification);
	}
	refreshText();
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
}

String PatchTextBox::makeHexDocument(std::shared_ptr<midikraft::PatchHolder> patch)
{
	if (!patch || !patch->patch())
		return "No patch active";

	String result;
	std::vector<uint8> binaryData = patch->patch()->data();

	uint8 *line = binaryData.data();
	int consumed = 0;
	while (consumed < (int)binaryData.size()) {
		uint8 posLSB = consumed & 0xff;
		uint8 posMSB = (consumed >> 8) & 0xff;
		result += String::toHexString(&posMSB, 1, 1) + String::toHexString(&posLSB, 1, 1) + " ";
		int lineLength = std::min(8, ((int)binaryData.size()) - consumed);
		String hex = String::toHexString(line, lineLength, 1);
		result += hex;
		result += "\n";

		line += lineLength;
		consumed += lineLength;
	}

	return result;
}

String PatchTextBox::makeTextDocument(std::shared_ptr<midikraft::PatchHolder> patch) {
	if (!patch || !patch->patch())
		return "No patch active";

	return patchToTextRaw(patch->patch(), false);
}

std::string PatchTextBox::patchToTextRaw(std::shared_ptr<midikraft::DataFile> patch, bool onlyActive)
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
				result = result + (boost::format("Layer: %s\n") % layers->layerName(layer)).str();
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
					result = result + (boost::format("%s: %s\n") % param->description() % param->valueInPatchToText(*patch)).str();
				}
			}
		}
	}
	return result;
}

