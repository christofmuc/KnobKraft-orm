/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "Synth.h"
#include "PatchHolder.h"

class DiffTokenizer;
class CoupledScrollCodeEditor;

class PatchDiff : public Component, private Button::Listener {
public:
	PatchDiff(midikraft::Synth *activeSynth, midikraft::PatchHolder const &patch1, midikraft::PatchHolder const &patch2);

	void resized() override;
	void buttonClicked(Button*) override;
	void buttonStateChanged(Button*) override;

private:
	void fillDocuments();
	static int positionInHexDocument(int positionInBinary);
	String makeHexDocument(midikraft::PatchHolder *patch);
	String makeTextDocument(midikraft::PatchHolder *patch);
	std::vector<Range<int>> diffFromText(String &doc1, String &doc2);
	std::vector<Range<int>> diffFromData(midikraft::Patch &patch1, midikraft::Patch &patch2);

	midikraft::Synth *activeSynth_;
	midikraft::PatchHolder p1_, p2_;
	std::unique_ptr<CodeDocument> p1Document_; 
	std::unique_ptr<CodeDocument> p2Document_;
	std::unique_ptr<DiffTokenizer> tokenizer1_;
	std::unique_ptr<DiffTokenizer> tokenizer2_;
	Label patch1Name_;
	Label patch2Name_;
	std::unique_ptr <CoupledScrollCodeEditor> p1Editor_;
	std::unique_ptr <CoupledScrollCodeEditor> p2Editor_;
	TextButton closeButton_;
	TextButton hexBased_, textBased_;

	bool showHexDiff_;
};
