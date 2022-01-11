/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PatchHolder.h"

class PatchTextBox : public Component {
public:
	enum class DisplayMode {
		HEX, PARAMS
	};
	PatchTextBox();

	void fillTextBox(std::shared_ptr<midikraft::PatchHolder> patch);

	virtual void resized() override;

	static String makeHexDocument(std::shared_ptr<midikraft::PatchHolder> patch);
	static String makeTextDocument(std::shared_ptr<midikraft::PatchHolder> patch);
	static std::string patchToTextRaw(std::shared_ptr<midikraft::DataFile> patch, bool onlyActive);

private:
	void refreshText();

	std::shared_ptr<midikraft::PatchHolder> patch_;
	std::unique_ptr<CodeDocument> document_;
	std::unique_ptr<CodeEditorComponent> textBox_;
	TextButton hexBased_, textBased_;
	DisplayMode mode_;
};



