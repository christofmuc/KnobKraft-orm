/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PluginSessionsWidget.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace knobkraft::sessions {

	namespace {
		juce::String synthStateText(SynthState state) {
			switch (state) {
			case SynthState::Unbound: return "Not bound";
			case SynthState::Missing: return "Missing";
			case SynthState::Offline: return "Offline";
			case SynthState::Online: return "Online";
			}
			return "Unknown";
		}

		juce::String shortFingerprint(std::optional<std::string> const& fingerprint) {
			if (!fingerprint) return "No fingerprint";
			auto text = juce::String(*fingerprint);
			return text.length() > 23 ? text.substring(0, 23) + "..." : text;
		}

		juce::String resultTime(std::int64_t unixMillis) {
			if (unixMillis <= 0) return {};
			return juce::Time(unixMillis).toString(true, true, true, true);
		}

		juce::Colour pillColour(PillMode mode) {
			switch (mode) {
			case PillMode::Disconnected: return juce::Colour(0xff4b5563);
			case PillMode::Connected: return juce::Colour(0xff287f63);
			case PillMode::ActiveTransfer: return juce::Colour(0xff236a9f);
			case PillMode::Attention: return juce::Colour(0xffa75a32);
			}
			return juce::Colours::darkgrey;
		}
	}

	class PluginSessionsDrawer final : public juce::Component, private juce::ListBoxModel {
	public:
		PluginSessionsDrawer(PluginSessionsController& controller, SessionsView view)
			: controller_(controller), view_(std::move(view)), list_("Plugin sessions", this) {
			title_.setText("Plugin Sessions", juce::dontSendNotification);
			title_.setFont(juce::Font(juce::FontOptions(18.0f).withStyle("Bold")));
			addAndMakeVisible(title_);
			list_.setRowHeight(104);
			list_.setOutlineThickness(1);
			list_.setColour(juce::ListBox::outlineColourId, juce::Colours::grey.withAlpha(0.45f));
			addAndMakeVisible(list_);

			cancel_.setButtonText("Cancel transfer");
			resolve_.setButtonText("Resolve");
			openSynth_.setButtonText("Open synth");
			openPatch_.setButtonText("Open patch");
			acknowledge_.setButtonText("Acknowledge");
			for (auto* button : { &cancel_, &resolve_, &openSynth_, &openPatch_, &acknowledge_ }) addAndMakeVisible(*button);
			cancel_.onClick = [this]() { withSelected([this](auto const& row) {
				if (row.activeOperation) showActionResult(controller_.cancel(row.pluginInstanceId, row.activeOperation->transferId));
			}); };
			resolve_.onClick = [this]() { withSelected([this](auto const& row) {
				showActionResult(controller_.navigate(row.pluginInstanceId, NavigationTarget::ResolveBinding));
			}); };
			openSynth_.onClick = [this]() { withSelected([this](auto const& row) {
				showActionResult(controller_.navigate(row.pluginInstanceId, NavigationTarget::Synth));
			}); };
			openPatch_.onClick = [this]() { withSelected([this](auto const& row) {
				showActionResult(controller_.navigate(row.pluginInstanceId, NavigationTarget::Patch));
			}); };
			acknowledge_.onClick = [this]() { withSelected([this](auto const& row) {
				controller_.acknowledgeAttention(row.pluginInstanceId);
			}); };
			setSize(610, 430);
			actionStatus_.setJustificationType(juce::Justification::centredLeft);
			addAndMakeVisible(actionStatus_);
			refreshButtons();
		}

		void setView(SessionsView view) {
			auto selectedPluginId = selected() ? selected()->pluginInstanceId : std::string {};
			view_ = std::move(view);
			list_.updateContent();
			if (!selectedPluginId.empty()) {
				auto const found = std::find_if(view_.rows.begin(), view_.rows.end(), [&](auto const& row) {
					return row.pluginInstanceId == selectedPluginId;
				});
				if (found != view_.rows.end()) list_.selectRow(static_cast<int>(std::distance(view_.rows.begin(), found)));
			}
			refreshButtons();
			repaint();
		}

		void resized() override {
			auto area = getLocalBounds().reduced(12);
			title_.setBounds(area.removeFromTop(28));
			area.removeFromTop(6);
			auto actions = area.removeFromBottom(30);
			actionStatus_.setBounds(area.removeFromBottom(24));
			constexpr int gap = 6;
			auto width = (actions.getWidth() - gap * 4) / 5;
			for (auto* button : { &cancel_, &resolve_, &openSynth_, &openPatch_, &acknowledge_ }) {
				button->setBounds(actions.removeFromLeft(width));
				actions.removeFromLeft(gap);
			}
			area.removeFromBottom(8);
			list_.setBounds(area);
		}

	private:
		int getNumRows() override { return static_cast<int>(view_.rows.size()); }

		void paintListBoxItem(int rowNumber, juce::Graphics& graphics, int width, int height, bool selectedRow) override {
			if (rowNumber < 0 || rowNumber >= getNumRows()) return;
			auto const& row = view_.rows[static_cast<std::size_t>(rowNumber)];
			auto area = juce::Rectangle<int>(0, 0, width, height).reduced(8, 5);
			if (selectedRow) graphics.fillAll(findColour(juce::ListBox::backgroundColourId).contrasting(0.08f));

			graphics.setColour(row.attention ? juce::Colours::orange : juce::Colours::white);
			graphics.setFont(juce::Font(juce::FontOptions(15.0f).withStyle("Bold")));
			auto heading = juce::String(row.instanceName) + "  [" + juce::String(row.pluginInstanceId) + "]";
			if (row.hostName) heading += "  Host: " + juce::String(*row.hostName);
			graphics.drawFittedText(heading, area.removeFromTop(20), juce::Justification::centredLeft, 1);

			graphics.setColour(juce::Colours::lightgrey);
			graphics.setFont(13.0f);
			graphics.drawFittedText("Synth: " + juce::String(row.synthName) + " (" + synthStateText(row.synthState) + ")",
				area.removeFromTop(18), juce::Justification::centredLeft, 1);
			graphics.drawFittedText("Project patch: " + juce::String(row.patchName.value_or("No patch")) + "  "
				+ shortFingerprint(row.patchFingerprint), area.removeFromTop(18), juce::Justification::centredLeft, 1);

			juce::String operation = "Idle";
			if (row.activeOperation) {
				operation = row.activeOperation->stateText;
				if (row.activeOperation->progress) operation += " " + juce::String(static_cast<int>(std::round(*row.activeOperation->progress * 100.0))) + "%";
				if (!row.activeOperation->detail.empty()) operation += ": " + juce::String(row.activeOperation->detail);
			}
			else if (row.lastResult) {
				operation = "Last: " + juce::String(row.lastResult->stateText);
				if (!row.lastResult->detail.empty()) operation += ": " + juce::String(row.lastResult->detail);
				operation += "  " + resultTime(row.lastResult->updatedAtUnixMillis);
			}
			graphics.drawFittedText(operation, area.removeFromTop(18), juce::Justification::centredLeft, 1);
			graphics.setColour(juce::Colours::grey);
			graphics.drawFittedText("Requested by plugin \"" + juce::String(row.instanceName) + "\" (" + juce::String(row.pluginInstanceId) + ")",
				area.removeFromTop(18), juce::Justification::centredLeft, 1);
		}

		void selectedRowsChanged(int) override { refreshButtons(); }

		template<typename Callback>
		void withSelected(Callback callback) {
			if (auto const* row = selected()) callback(*row);
		}

		SessionRow const* selected() const {
			auto const index = list_.getSelectedRow();
			return index >= 0 && index < static_cast<int>(view_.rows.size()) ? &view_.rows[static_cast<std::size_t>(index)] : nullptr;
		}

		void refreshButtons() {
			auto const* row = selected();
			cancel_.setEnabled(row && row->activeOperation && row->activeOperation->cancellable);
			resolve_.setEnabled(row && row->attention);
			openSynth_.setEnabled(row && row->configuredSynthInstanceId.has_value());
			openPatch_.setEnabled(row && row->patchFingerprint.has_value());
			acknowledge_.setEnabled(row && row->lastResult && row->lastResult->failed);
		}

		void showActionResult(ActionResult const& result) {
			actionStatus_.setColour(juce::Label::textColourId, result.accepted ? juce::Colours::lightgreen : juce::Colours::orange);
			actionStatus_.setText(result.message, juce::dontSendNotification);
		}

		PluginSessionsController& controller_;
		SessionsView view_;
		juce::Label title_;
		juce::ListBox list_;
		juce::TextButton cancel_;
		juce::TextButton resolve_;
		juce::TextButton openSynth_;
		juce::TextButton openPatch_;
		juce::TextButton acknowledge_;
		juce::Label actionStatus_;
	};

	PluginSessionsWidget::PluginSessionsWidget() {
		pill_.setButtonText("No plugins connected");
		pill_.setTooltip("Show plugin sessions");
		pill_.setColour(juce::TextButton::buttonColourId, pillColour(PillMode::Disconnected));
		pill_.onClick = [this]() { showDrawer(); };
		addAndMakeVisible(pill_);

		juce::Component::SafePointer<PluginSessionsWidget> safeThis(this);
		controller_.setViewChanged([safeThis]() {
			juce::MessageManager::callAsync([safeThis]() {
				if (safeThis) safeThis->updateFromController();
			});
		});
		startTimer(1000);
		updateFromController();
	}

	PluginSessionsWidget::~PluginSessionsWidget() {
		stopTimer();
		if (drawer_) {
			if (auto* callout = drawer_->findParentComponentOfClass<juce::CallOutBox>()) callout->dismiss();
			drawer_ = nullptr;
		}
		controller_.setViewChanged({});
		controller_.setService(nullptr);
	}

	void PluginSessionsWidget::setSessionService(midikraft::session::SessionService* service) { controller_.setService(service); }

	void PluginSessionsWidget::setSynths(std::vector<midikraft::session::SessionSynthInfo> synths) {
		controller_.setSynths(std::move(synths));
	}

	void PluginSessionsWidget::setNavigationHandler(PluginSessionsController::NavigationHandler handler) {
		controller_.setNavigationHandler(std::move(handler));
	}

	int PluginSessionsWidget::preferredWidth() const noexcept { return 210; }

	void PluginSessionsWidget::resized() { pill_.setBounds(getLocalBounds().reduced(3, 2)); }

	void PluginSessionsWidget::timerCallback() { controller_.tick(); }

	void PluginSessionsWidget::showDrawer() {
		if (drawer_) return;
		auto content = std::make_unique<PluginSessionsDrawer>(controller_, controller_.view());
		drawer_ = content.get();
		juce::CallOutBox::launchAsynchronously(std::move(content), pill_.getScreenBounds(), nullptr);
	}

	void PluginSessionsWidget::updateFromController() {
		auto const view = controller_.view();
		pill_.setButtonText(view.pillText);
		pill_.setColour(juce::TextButton::buttonColourId, pillColour(view.pillMode));
		pill_.setTooltip(view.rows.empty() ? "No plugin processors are connected" : "Show connected plugin sessions and transfers");
		if (drawer_) drawer_->setView(view);
	}

}
