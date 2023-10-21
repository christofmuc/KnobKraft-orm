/*
   Copyright (c) 2022 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "VerticalPatchButtonList.h"

#include "PatchHolderButton.h"
#include "LayoutConstants.h"

#include "UIModel.h"

#include <spdlog/spdlog.h>

typedef std::function<void(int, std::string const&, std::string const&)> TDragHighlightHandler;


class PatchListButtonWithMultiDrag : public PatchHolderButton
{
public:
	PatchListButtonWithMultiDrag(int id, bool isToggle, std::function<void(int)> clickHandler, TDragHighlightHandler dragHighlightHandler)
		: PatchHolderButton(id, isToggle, clickHandler)
		, dragHighlightHandler_(dragHighlightHandler)
	{
	}

	virtual void itemDragEnter(const SourceDetails& dragSourceDetails) override {
		String dropItemString = dragSourceDetails.description;
		auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());
		if (midikraft::PatchHolder::dragItemIsPatch(infos)) {
			PatchHolderButton::itemDragEnter(dragSourceDetails);
		}
		else if (midikraft::PatchHolder::dragItemIsList(infos)) {
			if (dragHighlightHandler_) {
				dragHighlightHandler_(0, infos["list_id"], infos["list_name"]);
			}
		}
	}

	virtual void itemDragExit(const SourceDetails& dragSourceDetails) override {
		String dropItemString = dragSourceDetails.description;
		auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());
		if (midikraft::PatchHolder::dragItemIsPatch(infos)) {
			PatchHolderButton::itemDragExit(dragSourceDetails);
		} else if (midikraft::PatchHolder::dragItemIsList(infos)) {
			if (dragHighlightHandler_) {
				dragHighlightHandler_(-1, "", "");
			}
		}
	}

private:
	TDragHighlightHandler dragHighlightHandler_;
};

class PatchButtonRow: public Component {
public:
	PatchButtonRow(std::function<void(int)> clickHandler, std::function<void(MidiProgramNumber, std::string)> patchChangeHandler, VerticalPatchButtonList::TListDropHandler listDropHandler, TDragHighlightHandler dragHighlightHandler)
		: clickHandler_(clickHandler) 
		, patchChangeHandler_(patchChangeHandler)
		, listDropHandler_(listDropHandler)
		, dragHighlightHandler_(dragHighlightHandler)
	{
	}

	virtual void resized() override {
		auto area = getLocalBounds();
		if (button_) {
			button_->setBounds(area);
		}
	}

	void setRow(int rowNo, midikraft::PatchHolder const &patch, bool dirty, PatchButtonInfo info) {
		// This changes the row to be displayed with this component (reusing components within a list box)
		if (!button_) {
			button_ = std::make_unique<PatchListButtonWithMultiDrag>(rowNo, false, clickHandler_, [this, rowNo](int zeroOrMinusOne, std::string const& list_id, std::string const& list_name) {
				dragHighlightHandler_(zeroOrMinusOne == -1 ? -1 : rowNo, list_id, list_name);
			});
			addAndMakeVisible(*button_);
			button_->acceptsItem = [this](juce::var dropItem) {
				String dropItemString = dropItem;
				auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());
				return (midikraft::PatchHolder::dragItemIsPatch(infos) && infos.contains("synth") && infos["synth"] == thePatch_.synth()->getName())
					|| midikraft::PatchHolder::dragItemIsList(infos);
			};
			button_->onItemDropped = [this](juce::var dropped) {
				String dropItemString = dropped;
				auto infos = midikraft::PatchHolder::dragInfoFromString(dropItemString.toStdString());
				if (midikraft::PatchHolder::dragItemIsPatch(infos)) {
					if (patchChangeHandler_ && infos.contains("md5")) {
						patchChangeHandler_(thePatch_.patchNumber(), infos["md5"]);
					}
					else {
						jassertfalse;
						spdlog::error("Program error - drag info has no md5 or no handler");
					}
				}
				else if (midikraft::PatchHolder::dragItemIsList(infos)) {
					listDropHandler_(thePatch_.patchNumber(), infos["list_id"], infos["list_name"]);
				}
			}; 

			resized();
		}
		else {
			button_->updateId(rowNo);
		}
		thePatch_ = patch; // Need a copy to keep the pointer alive
		button_->setPatchHolder(&thePatch_, info);
		button_->setDirty(dirty);
	}

	void clearRow() {
		if (button_) {
			button_.reset();
		}
	}

	midikraft::PatchHolder patch() const {
		return thePatch_;
	}

	// For custom highlighting
	PatchHolderButton* button() {
		if (button_) {
			return button_.get();
		}
		else {
			return nullptr;
		}
	}

private:
	std::unique_ptr<PatchHolderButton> button_;
	std::function<void(int)> clickHandler_;
	std::function<void(MidiProgramNumber, std::string)> patchChangeHandler_;
	VerticalPatchButtonList::TListDropHandler listDropHandler_;
	TDragHighlightHandler dragHighlightHandler_;
	midikraft::PatchHolder thePatch_;
};


class PatchListModel : public ListBoxModel {
public:
	PatchListModel(std::shared_ptr<midikraft::PatchList> bank, std::function<void(int)> onRowSelected,
			std::function<void(MidiProgramNumber, std::string)> patchChangeHandler, VerticalPatchButtonList::TListDropHandler listDropHandler, PatchButtonInfo info
			, TDragHighlightHandler dragHighlightHandler)
		: bank_(bank)
		, onRowSelected_(onRowSelected)
		, patchChangeHandler_(patchChangeHandler)
		, listDropHandler_(listDropHandler)
		, info_(info)
		, dragHighlightHandler_(dragHighlightHandler) {
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
		auto isBank = std::dynamic_pointer_cast<midikraft::SynthBank>(bank_);
		if (rowNumber < getNumRows()) {
			if (existingComponentToUpdate) {
				auto existing = dynamic_cast<PatchButtonRow*>(existingComponentToUpdate);
				if (existing) {
					existing->setRow(rowNumber, bank_->patches()[rowNumber], isBank ? isBank->isPositionDirty(rowNumber) : false, info_);
					return existing;
				}
				throw std::runtime_error("This was not the correct row type, can't continue");
			}
			auto newComponent = new PatchButtonRow(onRowSelected_, patchChangeHandler_, listDropHandler_, dragHighlightHandler_);
			newComponent->setRow(rowNumber, bank_->patches()[rowNumber], isBank ? isBank->isPositionDirty(rowNumber) : false, info_);
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
	std::shared_ptr<midikraft::PatchList> bank_;
	std::function<void(int)> onRowSelected_;
	std::function<void(MidiProgramNumber, std::string)> patchChangeHandler_;
	VerticalPatchButtonList::TListDropHandler listDropHandler_;
	PatchButtonInfo info_;
	TDragHighlightHandler dragHighlightHandler_;
};


VerticalPatchButtonList::VerticalPatchButtonList(std::function<void(MidiProgramNumber, std::string)> dropHandler, TListDropHandler listDropHandler, std::function<int(std::string const&, std::string const&)> listResolver) :
	dropHandler_(dropHandler)
	, listDropHandler_(listDropHandler)
	, listResolver_(listResolver)
{
	addAndMakeVisible(list_);
	list_.setRowHeight(LAYOUT_LARGE_LINE_SPACING);
}

void VerticalPatchButtonList::resized()
{
	auto bounds = getLocalBounds();
	list_.setBounds(bounds);
}

void VerticalPatchButtonList::refreshContent()
{
	list_.updateContent();
}

void VerticalPatchButtonList::clearList()
{
	list_.setModel(nullptr);
}

void VerticalPatchButtonList::setPatchList(std::shared_ptr<midikraft::PatchList> list, PatchButtonInfo info)
{
	list_.setModel(new PatchListModel(list, [this](int row) {
		auto patchRow = dynamic_cast<PatchButtonRow*>(list_.getComponentForRowNumber(row));
		if (patchRow) {
			if (onPatchClicked) {
				midikraft::PatchHolder copy = patchRow->patch();
				onPatchClicked(copy);
			}
			else {
				UIModel::instance()->currentPatch_.changeCurrentPatch(patchRow->patch());
				spdlog::debug("Patch {} selected", patchRow->patch().name());
			}
		}
		else {
			spdlog::error("No patch known for row {}", row);
		}
		},
		[this](MidiProgramNumber programPlace, std::string md5) {
			if (dropHandler_) {
				dropHandler_(programPlace, md5);
				list_.updateContent();
			}
		}
			, [this](MidiProgramNumber program, std::string const& list_id, std::string const& list_name) {
			if (listDropHandler_) {
				listDropHandler_(program, list_id, list_name);
			}
		}
			, info
			, [this, list](int startrow, std::string const& list_id, std::string const& list_name) {
			// This is a bit heavy, but the list itself has never been loaded...
			int rowCount = listResolver_(list_id, list_name);;
			for (int i = 0; i < list_.getListBoxModel()->getNumRows(); i++) {
				auto button = list_.getComponentForRowNumber(i);
				if (button != nullptr) {
					// This better be a PatchButton!
					auto pb = dynamic_cast<PatchButtonRow*>(button);
					if (pb) {
						if (pb->button()) {
							pb->button()->setGlow((i >= startrow && i < startrow + rowCount) && (startrow != -1));
						}
					}
					else {
						jassertfalse;
					}
				}
			}
		}));
}

