/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PluginEditor.h"

#include "PluginProcessor.h"

#include <array>

namespace knobkraft::recall {

	namespace {
		void configureLabel(juce::Component& parent, juce::Label& label, float fontSize, juce::Colour colour) {
			label.setFont(juce::Font(juce::FontOptions(fontSize)));
			label.setColour(juce::Label::textColourId, colour);
			label.setJustificationType(juce::Justification::centredLeft);
			parent.addAndMakeVisible(label);
		}
	}

	PluginEditor::PluginEditor(PluginProcessor& processor)
		: AudioProcessorEditor(processor), processor_(processor), progress_(progressValue_) {
		configureLabel(*this, title_, 24.0f, juce::Colours::white);
		configureLabel(*this, engineStatus_, 14.0f, juce::Colour(0xffaeb8c4));
		configureLabel(*this, bindingStatus_, 14.0f, juce::Colour(0xffaeb8c4));
		configureLabel(*this, patchName_, 19.0f, juce::Colour(0xff75d7b0));
		configureLabel(*this, fingerprint_, 12.0f, juce::Colour(0xff8d99a8));
		configureLabel(*this, provenance_, 12.0f, juce::Colour(0xff8d99a8));
		configureLabel(*this, storedStatus_, 14.0f, juce::Colour(0xff75d7b0));
		configureLabel(*this, transferStatus_, 13.0f, juce::Colour(0xffaeb8c4));
		configureLabel(*this, recallPolicy_, 13.0f, juce::Colour(0xffaeb8c4));
		configureLabel(*this, stateError_, 13.0f, juce::Colour(0xffff8d8d));
		std::array<juce::Component*, 12> components { &instanceName_, &openKnobKraft_, &synthPicker_, &rebind_,
			&patchSearch_, &search_, &patchPicker_, &choosePatch_, &morePatches_, &send_, &cancel_, &progress_ };
		for (auto* component : components)
			addAndMakeVisible(component);
		instanceName_.setTextToShowWhenEmpty("Plugin instance name", juce::Colour(0xff8d99a8));
		instanceName_.onReturnKey = [this] { commitInstanceName(); };
		instanceName_.onFocusLost = [this] { commitInstanceName(); };
		patchSearch_.setTextToShowWhenEmpty("Search KnobKraft patches", juce::Colour(0xff8d99a8));
		patchSearch_.onReturnKey = [this] { processor_.engineClient().searchPatches(patchSearch_.getText().toStdString()); };
		openKnobKraft_.onClick = [this] { processor_.engineClient().openKnobKraft(); };
		rebind_.onClick = [this] {
			auto const index = synthPicker_.getSelectedItemIndex();
			auto const state = processor_.engineClient().snapshot();
			if (index >= 0 && index < static_cast<int>(state->synths.size()))
				processor_.engineClient().rebind(state->synths[static_cast<std::size_t>(index)].configuredSynthInstanceId);
		};
		search_.onClick = [this] { processor_.engineClient().searchPatches(patchSearch_.getText().toStdString()); };
		choosePatch_.onClick = [this] {
			auto const index = patchPicker_.getSelectedItemIndex();
			auto const state = processor_.engineClient().snapshot();
			if (index >= 0 && index < static_cast<int>(state->patchResults.size()))
				processor_.engineClient().selectPatch(state->patchResults[static_cast<std::size_t>(index)].patchId);
		};
		morePatches_.onClick = [this] { processor_.engineClient().searchPatches(patchSearch_.getText().toStdString(), true); };
		send_.onClick = [this] { processor_.engineClient().sendStoredPatch(); };
		cancel_.onClick = [this] { processor_.engineClient().cancelTransfer(); };
		title_.setText("KnobKraft Recall", juce::dontSendNotification);
		processor_.engineClient().addChangeListener(this);
		setSize(700, 610);
		refresh();
	}

	PluginEditor::~PluginEditor() {
		processor_.engineClient().removeChangeListener(this);
	}

	void PluginEditor::paint(juce::Graphics& graphics) {
		graphics.fillAll(juce::Colour(0xff14191f));
		graphics.setColour(juce::Colour(0xff2a333d));
		graphics.fillRoundedRectangle(getLocalBounds().toFloat().reduced(16.0f).withTrimmedTop(72.0f), 8.0f);
	}

	void PluginEditor::resized() {
		auto bounds = getLocalBounds().reduced(24);
		title_.setBounds(bounds.removeFromTop(34));
		auto engineRow = bounds.removeFromTop(30);
		openKnobKraft_.setBounds(engineRow.removeFromRight(140));
		engineStatus_.setBounds(engineRow);
		bounds.removeFromTop(18);
		instanceName_.setBounds(bounds.removeFromTop(30));
		bounds.removeFromTop(8);
		bindingStatus_.setBounds(bounds.removeFromTop(24));
		auto synthRow = bounds.removeFromTop(30);
		rebind_.setBounds(synthRow.removeFromRight(90));
		synthRow.removeFromRight(8);
		synthPicker_.setBounds(synthRow);
		bounds.removeFromTop(12);
		auto searchRow = bounds.removeFromTop(30);
		morePatches_.setBounds(searchRow.removeFromRight(70));
		searchRow.removeFromRight(6);
		search_.setBounds(searchRow.removeFromRight(80));
		searchRow.removeFromRight(6);
		patchSearch_.setBounds(searchRow);
		bounds.removeFromTop(6);
		auto pickerRow = bounds.removeFromTop(30);
		choosePatch_.setBounds(pickerRow.removeFromRight(90));
		pickerRow.removeFromRight(8);
		patchPicker_.setBounds(pickerRow);
		bounds.removeFromTop(14);
		patchName_.setBounds(bounds.removeFromTop(34));
		fingerprint_.setBounds(bounds.removeFromTop(22));
		provenance_.setBounds(bounds.removeFromTop(22));
		storedStatus_.setBounds(bounds.removeFromTop(26));
		recallPolicy_.setBounds(bounds.removeFromTop(24));
		auto sendRow = bounds.removeFromTop(32);
		send_.setBounds(sendRow.removeFromLeft(140));
		sendRow.removeFromLeft(8);
		cancel_.setBounds(sendRow.removeFromLeft(90));
		sendRow.removeFromLeft(8);
		progress_.setBounds(sendRow);
		transferStatus_.setBounds(bounds.removeFromTop(36));
		stateError_.setBounds(bounds.removeFromTop(54));
	}

	void PluginEditor::changeListenerCallback(juce::ChangeBroadcaster*) {
		// ChangeBroadcaster delivers asynchronous notifications on JUCE's message
		// thread, so no component is ever touched by the IPC worker.
		refresh();
	}

	void PluginEditor::commitInstanceName() {
		processor_.engineClient().renameInstance(instanceName_.getText().trim().toStdString());
	}

	void PluginEditor::refresh() {
		auto const snapshot = processor_.state().snapshot();
		auto const& manifest = snapshot->manifest;
		if (!instanceName_.hasKeyboardFocus(true)) instanceName_.setText(juce::String(manifest.instanceName), false);
		auto const engine = processor_.engineClient().snapshot();
		juce::String engineText = "Engine: ";
		switch (engine->connection) {
		case EngineConnectionState::Connected: engineText += "Connected"; break;
		case EngineConnectionState::Connecting: engineText += "Connecting"; break;
		case EngineConnectionState::ProtocolIncompatible: engineText += "Incompatible protocol"; break;
		case EngineConnectionState::AuthenticationFailed: engineText += "Authentication failed"; break;
		case EngineConnectionState::Disconnected: engineText += "Disconnected"; break;
		}
		engineStatus_.setText(engineText + " - " + juce::String(engine->connectionDetail), juce::dontSendNotification);

		juce::String bindingText;
		switch (engine->binding) {
		case BindingState::Unbound: bindingText = "Hardware: Choose and bind a configured synth"; break;
		case BindingState::Resolving: bindingText = "Hardware: Resolving saved binding"; break;
		case BindingState::BoundOnline: bindingText = "Hardware: Bound and online"; break;
		case BindingState::BoundOffline: bindingText = "Hardware: Bound, but synth is offline"; break;
		case BindingState::Missing: bindingText = "Hardware: Saved synth is missing - select and Rebind"; break;
		case BindingState::AdaptationMismatch: bindingText = "Hardware: Adaptation changed - explicitly Rebind"; break;
		}
		bindingStatus_.setText(bindingText, juce::dontSendNotification);
		auto const priorSynth = synthPicker_.getText();
		synthPicker_.clear(juce::dontSendNotification);
		for (std::size_t i = 0; i < engine->synths.size(); ++i) {
			auto const& synth = engine->synths[i];
			synthPicker_.addItem(juce::String(synth.displayName) + (synth.online ? " (online)" : " (offline)"), static_cast<int>(i + 1));
			if (manifest.binding.configuredSynthInstanceId == synth.configuredSynthInstanceId)
				synthPicker_.setSelectedItemIndex(static_cast<int>(i), juce::dontSendNotification);
		}
		if (synthPicker_.getSelectedItemIndex() < 0 && priorSynth.isNotEmpty()) synthPicker_.setText(priorSynth, juce::dontSendNotification);
		auto const priorPatch = patchPicker_.getText();
		patchPicker_.clear(juce::dontSendNotification);
		for (std::size_t i = 0; i < engine->patchResults.size(); ++i)
			patchPicker_.addItem(juce::String(engine->patchResults[i].name), static_cast<int>(i + 1));
		if (!engine->patchResults.empty()) patchPicker_.setSelectedItemIndex(0, juce::dontSendNotification);
		else if (priorPatch.isNotEmpty()) patchPicker_.setText(priorPatch, juce::dontSendNotification);

		juce::String patchName = "No embedded project sound";
		juce::String fingerprint;
		juce::String provenance;
		if (manifest.selectedSoundId) {
			for (auto const& sound : manifest.sounds) {
				if (sound.soundId == *manifest.selectedSoundId) {
					patchName = juce::String(sound.patch.name);
					fingerprint = juce::String(sound.patch.fingerprint);
					if (sound.patch.source) {
						if (sound.patch.source->databaseId) provenance += "Database: " + juce::String(*sound.patch.source->databaseId) + "  ";
						if (sound.patch.source->bank) provenance += "Bank " + juce::String(*sound.patch.source->bank) + "  ";
						if (sound.patch.source->program) provenance += "Program " + juce::String(*sound.patch.source->program);
					}
					break;
				}
			}
		}
		patchName_.setText("Project sound: " + patchName, juce::dontSendNotification);
		fingerprint_.setText(fingerprint.isEmpty() ? juce::String() : "Fingerprint: " + fingerprint, juce::dontSendNotification);
		provenance_.setText(provenance, juce::dontSendNotification);
		storedStatus_.setText(snapshot->serializedState.empty() ? "Not stored in project" : "Stored in project", juce::dontSendNotification);
		recallPolicy_.setText("Recall policy: Manual", juce::dontSendNotification);
		progressValue_ = engine->transfer && engine->transfer->status.progress ? *engine->transfer->status.progress : 0.0;
		juce::String transferText = "No transfer requested";
		if (engine->transfer) {
			auto const& transfer = *engine->transfer;
			switch (transfer.status.state) {
			case midikraft::session::TransferState::Accepted: transferText = "Accepted"; break;
			case midikraft::session::TransferState::Queued: transferText = "Queued"; break;
			case midikraft::session::TransferState::Preparing: transferText = "Preparing"; break;
			case midikraft::session::TransferState::Sending: transferText = "Sending"; break;
			case midikraft::session::TransferState::Verifying: transferText = "Verifying"; break;
			case midikraft::session::TransferState::Succeeded:
				transferText = transfer.status.verification == midikraft::session::VerificationState::Verified ? "Verified" : "Sent"; break;
			case midikraft::session::TransferState::Failed: transferText = "Failed"; break;
			case midikraft::session::TransferState::Cancelled: transferText = "Cancelled"; break;
			}
			transferText += ": " + juce::String(transfer.status.detail);
			if (engine->lastResultUnixMillis) {
				auto time = juce::Time(*engine->lastResultUnixMillis);
				transferText += " at " + time.formatted("%H:%M:%S");
			}
		}
		transferStatus_.setText(transferText, juce::dontSendNotification);
		progress_.repaint();
		send_.setEnabled(engine->canSend() && !snapshot->hasDecodeError());
		cancel_.setEnabled(engine->canCancel());
		rebind_.setEnabled(engine->connection == EngineConnectionState::Connected && synthPicker_.getNumItems() > 0 && !snapshot->hasDecodeError());
		search_.setEnabled(engine->connection == EngineConnectionState::Connected && !snapshot->hasDecodeError());
		choosePatch_.setEnabled(!engine->patchResults.empty() && !snapshot->hasDecodeError());
		morePatches_.setEnabled(engine->nextPatchPageToken.has_value());

		if (snapshot->decodeError) {
			auto const& error = *snapshot->decodeError;
			stateError_.setText("State error at " + juce::String(error.path) + ": " + juce::String(error.message)
				+ (error.rawStatePreserved ? " (raw state preserved)" : ""), juce::dontSendNotification);
		}
		else if (engine->error) {
			stateError_.setText("Action needed: " + juce::String(engine->error->message)
				+ (engine->error->retryable ? " (retry when ready)" : ""), juce::dontSendNotification);
		}
		else {
			stateError_.setText("Stored is not the same as sent. Recall occurs only when you press Send.", juce::dontSendNotification);
		}
	}

}
