/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "PluginSessionsModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace knobkraft::sessions {

	class PluginSessionsDrawer;

	class PluginSessionsWidget final : public juce::Component, private juce::Timer {
	public:
		PluginSessionsWidget();
		~PluginSessionsWidget() override;

		void setSessionService(midikraft::session::SessionService* service);
		void setSynths(std::vector<midikraft::session::SessionSynthInfo> synths);
		void setNavigationHandler(PluginSessionsController::NavigationHandler handler);
		[[nodiscard]] int preferredWidth() const noexcept;

		void resized() override;

	private:
		void timerCallback() override;
		void showDrawer();
		void updateFromController();

		juce::TextButton pill_;
		PluginSessionsController controller_;
		juce::Component::SafePointer<PluginSessionsDrawer> drawer_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSessionsWidget)
	};

}
