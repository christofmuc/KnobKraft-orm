/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchDiff.h"

#include "Synth.h"
#include "PatchHolder.h"
#include "Patch.h"

#include <algorithm>
#include "dtl/dtl.hpp"

class DiffTokenizer : public CodeTokeniser {
public:
	DiffTokenizer(CodeDocument &doc) : document_(doc) {}

	void setRangeList(std::vector<Range<int>> &ranges) {
		ranges_ = ranges;
	}

	virtual int readNextToken(CodeDocument::Iterator& source) override
	{
		// Determine if this is the start of a diff region
		for (auto range : ranges_) {
			if (source.getPosition() == range.getStart()) {
				// Hit! Consume enough characters to move ahead
				for (int i = 0; i < range.getLength(); i++) source.skip();
				return DIFF;
			}
		}
		// No hit, advance iterator
		source.skip();
		return PLAIN;
	}

	virtual CodeEditorComponent::ColourScheme getDefaultColourScheme() override
	{
		CodeEditorComponent::ColourScheme result;
		result.set("Plain", Colours::beige);
		result.set("Diff", Colours::indianred);
		return result;
	}

private:
	enum {
		PLAIN,
		DIFF
	};
	CodeDocument & document_;
	std::vector<Range<int>> ranges_;
};

PatchDiff::PatchDiff(midikraft::Synth *activeSynth, midikraft::PatchHolder const &patch1, midikraft::PatchHolder const &patch2) : p1_(patch1), p2_(patch2),
	activeSynth_(activeSynth), p1Document_(new CodeDocument), p2Document_(new CodeDocument), showHexDiff_(false)
{
	// Create more components, with more complex bootstrapping
	tokenizer1_.reset(new DiffTokenizer(*p1Document_.get()));
	tokenizer2_.reset(new DiffTokenizer(*p2Document_.get()));
	p1Editor_.reset(new CodeEditorComponent(*p1Document_, tokenizer1_.get()));
	p2Editor_.reset(new CodeEditorComponent(*p2Document_, tokenizer2_.get()));

	fillDocuments();

	// Close button for dialog
	closeButton_.setButtonText("Close");
	closeButton_.addListener(this);
	addAndMakeVisible(closeButton_);

	// Header Labels
	addAndMakeVisible(patch1Name_);
	addAndMakeVisible(patch2Name_);

	// Init the editors
	p1Editor_->setReadOnly(true);
	p2Editor_->setReadOnly(true);
	addAndMakeVisible(*p1Editor_);
	addAndMakeVisible(*p2Editor_);

	// Build the toggle buttons for the diff mode
	addAndMakeVisible(hexBased_);
	addAndMakeVisible(textBased_);
	hexBased_.setButtonText("Show hex values");
	hexBased_.setClickingTogglesState(true);
	hexBased_.setRadioGroupId(3, dontSendNotification);
	hexBased_.addListener(this);
	textBased_.setButtonText("Show parameter values");
	textBased_.setToggleState(true, dontSendNotification);
	textBased_.setRadioGroupId(3, dontSendNotification);
	textBased_.setClickingTogglesState(true);
	textBased_.addListener(this);

	// Finally we need a default size
	setBounds(0, 0, 540, 600);
}

void PatchDiff::resized()
{
	Rectangle<int> area(getLocalBounds());
	closeButton_.setBounds(area.removeFromBottom(20).withSizeKeepingCentre(100, 20));
	auto topRow = area.removeFromTop(20);
	hexBased_.setBounds(topRow.removeFromLeft(100));
	textBased_.setBounds(topRow.removeFromLeft(100));
	auto leftColumn = area.removeFromLeft(area.getWidth() / 2);
	patch1Name_.setBounds(leftColumn.removeFromTop(30));
	p1Editor_->setBounds(leftColumn);
	patch2Name_.setBounds(area.removeFromTop(30));
	p2Editor_->setBounds(area);
}

void PatchDiff::buttonClicked(Button *button)
{
	if (button == &closeButton_) {
		if (DialogWindow* dw = findParentComponentOfClass<DialogWindow>()) {
			dw->exitModalState(true);
		}
	} 
}

void PatchDiff::buttonStateChanged(Button *button)
{
	if (button->getToggleState()) {
		if (button == &hexBased_) {
			showHexDiff_ = true;
			fillDocuments();
		}
		else if (button == &textBased_) {
			showHexDiff_ = false;
			fillDocuments();
		}
	}
}

void PatchDiff::fillDocuments()
{
	patch1Name_.setText(p1_.patch()->patchName(), dontSendNotification);
	patch2Name_.setText(p2_.patch()->patchName(), dontSendNotification);

	String doc1, doc2;
	if (showHexDiff_) {
		doc1 = makeHexDocument(&p1_);
		doc2 = makeHexDocument(&p2_);
		std::vector<Range<int>> diffRanges = diffFromData(*p1_.patch(), *p2_.patch());
		tokenizer1_->setRangeList(diffRanges);
		tokenizer2_->setRangeList(diffRanges);
	}
	else {
		doc1 = makeTextDocument(&p1_);
		doc2 = makeTextDocument(&p2_);
		std::vector<Range<int>> diffRanges1 = diffFromText(doc1, doc2);
		std::vector<Range<int>> diffRanges2 = diffFromText(doc2, doc1);
		tokenizer1_->setRangeList(diffRanges1);
		tokenizer2_->setRangeList(diffRanges2);
	}

	// Setup view
	p1Document_->replaceAllContent(doc1);
	p2Document_->replaceAllContent(doc2);
}

int PatchDiff::positionInHexDocument(int positionInBinary) {
	int row = positionInBinary / 8;
	int column = positionInBinary % 8;
	int headerLength = 5;
	int columnWidth = 3;
	int rowLength = headerLength + 8 * columnWidth;
	return row * rowLength + headerLength + column * columnWidth;
}

String PatchDiff::makeHexDocument(midikraft::PatchHolder *patch)
{
	String result;
	std::vector<uint8> binaryData = patch->patch()->data();

	uint8 *line = binaryData.data();
	int consumed = 0;
	while (consumed < (int) binaryData.size()) {
		uint8 posLSB = consumed & 0xff;
		uint8 posMSB = (consumed >> 8) & 0xff;
		result += String::toHexString(&posMSB, 1, 1) + String::toHexString(&posLSB, 1, 1) + " ";
		int lineLength = std::min(8, ((int) binaryData.size()) - consumed);
		String hex = String::toHexString(line, lineLength, 1);
		result += hex;
		result += "\n";

		line += lineLength;
		consumed += lineLength;
	}

	return result;
}

String PatchDiff::makeTextDocument(midikraft::PatchHolder *patch) {
	// Use the generic parameter list mode of the patch itself. Not pretty, but does the job for now
	return patch->patch()->patchToTextRaw(false);
}

std::vector<Range<int>> PatchDiff::diffFromText(String &doc1, String &doc2) {
	dtl::Diff< char, std::string> difference(doc1.toStdString(), doc2.toStdString());
	difference.compose();

	// Diff calculation for highlighting
	std::vector<Range<int>> diffRanges;

	// Loop over edit script and create highlighting ranges
	auto editScript = difference.getSes();
	for (auto edit : editScript.getSequence()) {
	if (edit.second.type == dtl::SES_DELETE) {
			diffRanges.push_back(Range<int>((int) (edit.second.beforeIdx - 1), (int) edit.second.beforeIdx));
		}
	}

	return diffRanges;
}

std::vector<Range<int>> PatchDiff::diffFromData(midikraft::Patch &patch1, midikraft::Patch &patch2) {
	// Diff calculation for highlighting
	std::vector<Range<int>> diffRanges;
	bool diff = false;
	int diffstart = 0;
	std::vector<uint8> const &doc1 = activeSynth_->filterVoiceRelevantData(patch1.data());
	std::vector<uint8> const &doc2 = activeSynth_->filterVoiceRelevantData(patch2.data());

	for (size_t i = 0; i < doc1.size(); i++) {
		if (doc1[i] != doc2[i]) {
			if (!diff) {
				diff = true;
				diffstart = positionInHexDocument(i);
			}
			if (i % 8 == 7) {
				// End of line, need to end the diff range else the diff will include the header of the line
				diff = false;
				diffRanges.push_back(Range<int>(diffstart, positionInHexDocument(i) + 2));
			}
		}
		else if (diff) {
			diff = false;
			diffRanges.push_back(Range<int>(diffstart, positionInHexDocument(i)));
		}
	}
	if (diff) {
		// Open diff at the end of the document
		diffRanges.push_back(Range<int>(diffstart, positionInHexDocument(doc1.size())));
	}
	return diffRanges;
}
