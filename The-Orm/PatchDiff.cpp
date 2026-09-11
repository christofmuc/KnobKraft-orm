/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PatchDiff.h"

#include "Synth.h"
#include "PatchHolder.h"
#include "Patch.h"
#include "Capability.h"

#include "DetailedParametersCapability.h"
#include "PatchTextBox.h"
#include "TextDiffRanges.h"

#include <algorithm>

class DiffTokenizer : public CodeTokeniser {
public:
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
	std::vector<Range<int>> ranges_;
};

class CoupledScrollCodeEditor : public CodeEditorComponent {
public:
	using CodeEditorComponent::CodeEditorComponent;

	void setSlavedEditor(CodeEditorComponent *editor) {
		slave_ = editor;
	}

	void editorViewportPositionChanged() override {
		if (slave_) {
			slave_->scrollToLine(getFirstLineOnScreen());
			//slave_->scrollToColumn();
		}
	}

private:
	CodeEditorComponent *slave_ = nullptr;
};

PatchDiff::PatchDiff(midikraft::Synth *activeSynth, midikraft::PatchHolder const &patch1, midikraft::PatchHolder const &patch2) : activeSynth_(activeSynth), p1_(patch1), p2_(patch2),
	p1Document_(new CodeDocument), p2Document_(new CodeDocument)
{
	// Create more components, with more complex bootstrapping
	tokenizer1_.reset(new DiffTokenizer());
	tokenizer2_.reset(new DiffTokenizer());
	p1Editor_.reset(new CoupledScrollCodeEditor(*p1Document_, tokenizer1_.get()));
	p2Editor_.reset(new CoupledScrollCodeEditor(*p2Document_, tokenizer2_.get()));
	p1Editor_->setSlavedEditor(p2Editor_.get());
	p2Editor_->setSlavedEditor(p1Editor_.get());
	//TODO - I wish I could hide the scrollbar on the left editor, but I can only hide horizontal and vertical at the same time
	// As the horizontal slaving doesn't work, just keep the ugly scrollbars for now
	//p1Editor_->setScrollbarThickness(0);
	p1Editor_->setScrollbarThickness(10);
	p2Editor_->setScrollbarThickness(10);

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

	// Built-in views and adaptation-provided views share one selector.
	addAndMakeVisible(viewSelector_);
	viewSelector_.addItem("Raw hex", 1);
	auto parameterDetails = midikraft::Capability::hasCapability<midikraft::DetailedParametersCapability>(patch1.patch());
	auto parameterDetails2 = midikraft::Capability::hasCapability<midikraft::DetailedParametersCapability>(patch2.patch());
	if (parameterDetails && parameterDetails2) viewSelector_.addItem("Parameter values", 2);
	customViews_ = patch_text::commonViews(patch_text::viewsFor(p1_), patch_text::viewsFor(p2_));
	for (size_t i = 0; i < customViews_.size(); ++i) {
		viewSelector_.addItem(String::fromUTF8(customViews_[i].name.c_str()), 3 + static_cast<int>(i));
	}
	viewSelector_.setSelectedId(parameterDetails && parameterDetails2 ? 2 : 1, dontSendNotification);
	viewSelector_.onChange = [this]() { fillDocuments(); };

	fillDocuments();

	// Finally we need a default size
	setBounds(0, 0, 540, 600);
}

PatchDiff::~PatchDiff()
{
}

void PatchDiff::resized()
{
	Rectangle<int> area(getLocalBounds());
	closeButton_.setBounds(area.removeFromBottom(20).withSizeKeepingCentre(100, 20));
	auto topRow = area.removeFromTop(20);
	viewSelector_.setBounds(topRow);
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

void PatchDiff::fillDocuments()
{
	patch1Name_.setText(p1_.name(), dontSendNotification);
	patch2Name_.setText(p2_.name(), dontSendNotification);

	String doc1, doc2;
	if (viewSelector_.getSelectedId() == 1) {
		doc1 = makeHexDocument(&p1_);
		doc2 = makeHexDocument(&p2_);
		std::vector<Range<int>> diffRanges = diffFromData(p1_.patch(), p2_.patch());
		tokenizer1_->setRangeList(diffRanges);
		tokenizer2_->setRangeList(diffRanges);
	}
	else {
		if (viewSelector_.getSelectedId() == 2) {
			doc1 = PatchTextBox::makeTextDocument(std::make_shared<midikraft::PatchHolder>(p1_));
			doc2 = PatchTextBox::makeTextDocument(std::make_shared<midikraft::PatchHolder>(p2_));
		}
		else {
			auto const& view = customViews_.at(static_cast<size_t>(viewSelector_.getSelectedId() - 3));
			doc1 = String::fromUTF8(view.left.c_str());
			doc2 = String::fromUTF8(view.right.c_str());
		}
		auto ranges = patch_text::diffRanges(doc1, doc2);
		tokenizer1_->setRangeList(ranges.left);
		tokenizer2_->setRangeList(ranges.right);
	}

	// Keep supplied whitespace and line endings verbatim. applyChanges() normalizes them.
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

std::vector<Range<int>> PatchDiff::diffFromData(std::shared_ptr<midikraft::DataFile> patch1, std::shared_ptr<midikraft::DataFile> patch2) {
	// Diff calculation for highlighting
	std::vector<Range<int>> diffRanges;
	bool diff = false;
	int diffstart = 0;
	std::vector<uint8> const &doc1 = activeSynth_->filterVoiceRelevantData(patch1);
	std::vector<uint8> const &doc2 = activeSynth_->filterVoiceRelevantData(patch2);

	for (int i = 0; i < (int) std::min(doc1.size(), doc2.size()); i++) {
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
		diffRanges.push_back(Range<int>(diffstart, positionInHexDocument((int) doc1.size())));
	}
	return diffRanges;
}

