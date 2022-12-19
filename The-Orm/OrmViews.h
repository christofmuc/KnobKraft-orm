#pragma once

#include "AutoDetection.h"
#include "Librarian.h"

#include "PatchDatabase.h"
#include "AutomaticCategory.h"

#pragma warning( push )
#pragma warning( disable : 4100 )
#include <docks/docks.h>
#pragma warning( pop )

class OrmViews : public DockManager::Delegate
{
public:
	OrmViews();
	static OrmViews& instance();
	static void shutdown();

	// Some global logic objects that we need only once
	midikraft::Librarian& librarian();
	midikraft::AutoDetection& autoDetector();
	midikraft::PatchDatabase& patchDatabase();
	std::shared_ptr<midikraft::AutomaticCategory> automaticCategories();

	// A little bit of extra state - this needs refactoring
	void reloadAutomaticCategories();

	// Implementation of DockManager::Delegate
	virtual const juce::StringArray getAvailableViews() const override;
	virtual std::shared_ptr<juce::Component> createView(const juce::String& nameOfViewToCreate) override;
	virtual const juce::String getDefaultWindowName() const override;
	virtual std::shared_ptr<DockingWindow> createTopLevelWindow(DockManager& manager, DockManagerData& data, const juce::ValueTree& tree) override;

private:
	midikraft::Librarian librarian_;
	midikraft::AutoDetection autodetector_;
	std::unique_ptr<midikraft::PatchDatabase> database_;
	std::shared_ptr<midikraft::AutomaticCategory> automaticCategories_;

	static std::unique_ptr<OrmViews> instance_;
};
