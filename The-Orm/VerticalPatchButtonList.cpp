/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "VerticalPatchButtonList.h"

#include "PatchHolderButton.h"
#include "LayoutConstants.h"

class PatchButtonRow: public Component {
public:
	PatchButtonRow(std::function<void(int)> clickHandler, std::function<void(MidiProgramNumber, std::string)> patchChangeHandler) 
		: clickHandler_(clickHandler) 
		, patchChangeHandler_(patchChangeHandler)
	{
	}

	virtual void resized() override {
		auto area = getLocalBounds();
		if (button_)
			button_->setBounds(area);
	}

	void setRow(int rowNo, midikraft::PatchHolder const &patch, PatchButtonInfo info) {
		// This changes the row to be displayed with this component (reusing components within a list box)
		if (!button_) {
			button_ = std::make_unique<PatchHolderButton>(rowNo, false, clickHandler_);
			addAndMakeVisible(*button_);
			button_->acceptsItem = [this](juce::var dropItem) {
				String dropItemString = dropItem;
				auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());
				return midikraft::PatchHolder::dragItemIsPatch(infos) && infos.contains("synth") && infos["synth"] == thePatch_.synth()->getName();
			};
			button_->onItemDropped = [this](juce::var dropped) {
				String dropItemString = dropped;
				auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());
				if (patchChangeHandler_ && infos.contains("md5")) {
					patchChangeHandler_(thePatch_.patchNumber(), infos["md5"]);
				}
				else {
					jassertfalse;
					SimpleLogger::instance()->postMessage("Program error - drag info has no md5 or no handler");
				}
			}; 
		
			resized();
		}
		else {
			button_->updateId(rowNo);
		}
		thePatch_ = patch; // Need a copy to keep the pointer alive
		button_->setPatchHolder(&thePatch_, false, info);
	}

	void clearRow() {
		if (button_) {
			button_.reset();
		}
	}

	midikraft::PatchHolder patch() const {
		return thePatch_;
	}

private:
	std::unique_ptr<PatchHolderButton> button_;
	std::function<void(int)> clickHandler_;
	std::function<void(MidiProgramNumber, std::string)> patchChangeHandler_;
	midikraft::PatchHolder thePatch_;
};


class PatchListModel : public ListBoxModel {
public:
	PatchListModel(std::shared_ptr<midikraft::SynthBank> bank, std::function<void(int)> onRowSelected,
			std::function<void(MidiProgramNumber, std::string)> patchChangeHandler, PatchButtonInfo info)
		: bank_(bank), onRowSelected_(onRowSelected), patchChangeHandler_(patchChangeHandler), info_(info) {
	}

	int getNumRows() override
	{
		return (int) bank_->patches().size();
	}

	void paintListBoxItem(int rowNumber, Graphics& g, int width, int height, bool rowIsSelected) override
	{
		ignoreUnused(rowNumber, width, height, rowIsSelected);
		g.fillAll();
	}

	Component* refreshComponentForRow(int rowNumber, bool isRowSelected, Component* existingComponentToUpdate) override
	{
		ignoreUnused(isRowSelected);
		if (rowNumber < getNumRows()) {
			if (existingComponentToUpdate) {
				auto existing = dynamic_cast<PatchButtonRow*>(existingComponentToUpdate);
				if (existing) {
					existing->setRow(rowNumber, bank_->patches()[rowNumber], info_);
					return existing;
				}
				throw std::runtime_error("This was not the correct row type, can't continue");
			}
			auto newComponent = new PatchButtonRow(onRowSelected_, patchChangeHandler_);
			newComponent->setRow(rowNumber, bank_->patches()[rowNumber], info_);
			return newComponent;
		}
		else {
			if (existingComponentToUpdate) {
				auto existing = dynamic_cast<PatchButtonRow*>(existingComponentToUpdate);
				if (existing) {
					existing->clearRow();
					return existing;
				}
				throw std::runtime_error("This was not the correct row type, can't continue");
			}
			return nullptr;
		}
	}

	

private:
	std::shared_ptr<midikraft::SynthBank> bank_;
	std::function<void(int)> onRowSelected_;
	std::function<void(MidiProgramNumber, std::string)> patchChangeHandler_;
	PatchButtonInfo info_;
};


VerticalPatchButtonList::VerticalPatchButtonList(std::function<void(MidiProgramNumber, std::string)> dropHandler) : dropHandler_(dropHandler)
{
	addAndMakeVisible(list_);
	list_.setRowHeight(LAYOUT_LARGE_LINE_SPACING);
}

void VerticalPatchButtonList::resized()
{
	auto bounds = getLocalBounds();
	list_.setBounds(bounds);
}

void VerticalPatchButtonList::setPatches(std::shared_ptr<midikraft::SynthBank> bank, PatchButtonInfo info)
{
	list_.setModel(new PatchListModel(bank, [this](int row) {
		auto patchRow = dynamic_cast<PatchButtonRow *>(list_.getComponentForRowNumber(row));
		if (patchRow) {
			SimpleLogger::instance()->postMessage("Patch " + patchRow->patch().name()  + "selected");
		}
		else {
			SimpleLogger::instance()->postMessage("No patch known for row " + String(row));
		}
	},
		[this](MidiProgramNumber programPlace, std::string md5) {
		if (dropHandler_) {
			dropHandler_(programPlace, md5);
			list_.updateContent();
		}
	}
	, info));
}
