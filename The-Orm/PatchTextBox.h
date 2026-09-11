/*
   Copyright (c) 2021 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "PatchHolder.h"
#include "PatchTextCapability.h"

class PatchTextBox : public Component {
public:
	PatchTextBox(std::function<void()> forceResize = {}, bool showParams = true);

	void fillTextBox(std::shared_ptr<midikraft::PatchHolder> patch);

	virtual void resized() override;

	String makeHexDocument(std::shared_ptr<midikraft::PatchHolder> patch);
	static String makeTextDocument(std::shared_ptr<midikraft::PatchHolder> patch);
	static std::string patchToTextRaw(std::shared_ptr<midikraft::Patch> patch, bool onlyActive);

	float desiredHeight() const;

private:
	void refreshText();

	std::function<void()> forceResize_;
	bool showParams_;
	std::shared_ptr<midikraft::PatchHolder> patch_;
	std::unique_ptr<CodeDocument> document_;
	std::unique_ptr<CodeEditorComponent> textBox_;
	TextButton hexBased_;
	ComboBox viewSelector_;
	midikraft::PatchTextViews customViews_;
	std::optional<int> lastLayoutedWidth_;
};



