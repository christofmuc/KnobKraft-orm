/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "KnobKraftEngineBackend.h"

#include "Capability.h"
#include "EditBufferCapability.h"
#include "MidiController.h"
#include "PatchDatabase.h"
#include "PatchFilter.h"
#include "PatchHolder.h"
#include "SimpleDiscoverableDevice.h"
#include "Synth.h"
#include "UIModel.h"

#include <juce_events/juce_events.h>

#include <future>
#include <limits>
#include <stdexcept>

namespace knobkraft::recall {
	using namespace midikraft::session;

	namespace {
		template<typename Function>
		auto onMessageThread(Function&& function) -> decltype(function()) {
			using Result = decltype(function());
			auto* messageManager = juce::MessageManager::getInstanceWithoutCreating();
			if (!messageManager || messageManager->isThisTheMessageThread()) return function();
			auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<Function>(function));
			auto future = task->get_future();
			if (!juce::MessageManager::callAsync([task] { (*task)(); })) throw std::runtime_error("KnobKraft message thread is unavailable");
			return future.get();
		}

		std::shared_ptr<midikraft::Synth> synthForAdaptation(std::string const& adaptationId) {
			for (auto holder : UIModel::instance()->synthList_.allSynths()) {
				auto synth = holder.synth();
				if (synth && synth->getName() == adaptationId) return synth;
			}
			return {};
		}

		std::shared_ptr<midikraft::Synth> synthForInstance(std::string const& instanceId) {
			for (auto holder : UIModel::instance()->synthList_.allSynths()) {
				auto device = holder.device();
				if (device && device->configuredSynthInstanceId() == instanceId) return holder.synth();
			}
			return {};
		}

		std::string patchId(midikraft::PatchHolder const& patch) {
			return patch.smartSynth()->getName() + ":" + patch.md5();
		}

		EnginePatch project(midikraft::PatchHolder const& patch) {
			EnginePatch result;
			result.patchId = patchId(patch);
			result.adaptationId = patch.smartSynth()->getName();
			result.dataTypeId = std::to_string(patch.getType());
			result.name = patch.name();
			result.payload = patch.patch()->data();
			PatchProvenance source;
			source.databaseId = patch.md5();
			if (patch.bankNumber().isValid()) source.bank = patch.bankNumber().toZeroBased();
			if (patch.patchNumber().isValid()) source.program = patch.patchNumber().toZeroBasedDiscardingBank();
			result.source = std::move(source);
			return result;
		}

		ServiceResult<bool> error(ServiceErrorCode code, std::string message, bool retryable = false) {
			return ServiceResult<bool>::failure({ code, std::move(message), retryable });
		}
	}

	KnobKraftEngineBackend::KnobKraftEngineBackend(midikraft::PatchDatabase& database, std::function<void()> showApplication)
		: database_(database), showApplication_(std::move(showApplication)) {}

	std::vector<midikraft::session::ConfiguredSynthInstance> KnobKraftEngineBackend::configuredSynths() {
		return onMessageThread([] { return UIModel::instance()->configuredSynths_.all(); });
	}

	PatchSearchPage KnobKraftEngineBackend::searchPatches(std::string const& query,
		std::optional<std::string> const& adaptationId, std::size_t offset, std::size_t limit) {
		return onMessageThread([this, query, adaptationId, offset, limit] {
			std::vector<std::shared_ptr<midikraft::Synth>> synths;
			if (adaptationId) {
				auto synth = synthForAdaptation(*adaptationId);
				if (!synth) return PatchSearchPage {};
				synths.push_back(std::move(synth));
			}
			else {
				for (auto const& configured : UIModel::instance()->configuredSynths_.all()) {
					auto synth = synthForAdaptation(configured.adaptationId);
					if (synth) synths.push_back(std::move(synth));
				}
			}
			if (synths.empty()) return PatchSearchPage {};
			midikraft::PatchFilter filter(synths);
			filter.name = query;
			filter.orderBy = midikraft::PatchOrdering::Order_by_Name;
			auto safeOffset = static_cast<int>(std::min(offset, static_cast<std::size_t>(std::numeric_limits<int>::max())));
			auto safeLimit = static_cast<int>(std::min(limit + 1, static_cast<std::size_t>(std::numeric_limits<int>::max())));
			auto patches = database_.getPatches(filter, safeOffset, safeLimit);
			PatchSearchPage result;
			result.hasMore = patches.size() > limit;
			if (result.hasMore) patches.resize(limit);
			for (auto const& patch : patches) if (patch.patch() && patch.smartSynth()) result.patches.push_back(project(patch));
			return result;
		});
	}

	std::optional<EnginePatch> KnobKraftEngineBackend::getPatch(std::string const& id) {
		return onMessageThread([this, id]() -> std::optional<EnginePatch> {
			auto separator = id.rfind(':');
			if (separator == std::string::npos || separator + 1 == id.size()) return std::nullopt;
			auto synth = synthForAdaptation(id.substr(0, separator));
			if (!synth) return std::nullopt;
			std::vector<midikraft::PatchHolder> patches;
			if (!database_.getSinglePatch(synth, id.substr(separator + 1), patches) || patches.empty()) return std::nullopt;
			return project(patches.front());
		});
	}

	ServiceResult<bool> KnobKraftEngineBackend::sendToEditBuffer(std::string const& instanceId,
		SessionPatch const& patch, std::atomic_bool const& cancelled, Progress progress) {
		try {
			return onMessageThread([instanceId, patch, &cancelled, progress = std::move(progress)]() mutable -> ServiceResult<bool> {
				auto synth = synthForInstance(instanceId);
				if (!synth) return error(ServiceErrorCode::ConfiguredSynthMissing, "Configured synth runtime was not found");
				auto device = std::dynamic_pointer_cast<midikraft::SimpleDiscoverableDevice>(synth);
				if (!device || !device->wasDetected()) return error(ServiceErrorCode::SynthOffline, "Synth is offline", true);
				auto editBuffer = midikraft::Capability::hasCapability<midikraft::EditBufferCapability>(synth);
				if (!editBuffer) return error(ServiceErrorCode::PatchIncompatible, "Synth has no edit-buffer capability");
				if (cancelled.load()) return error(ServiceErrorCode::TransferCancelled, "Transfer cancelled");
				progress(0.2, "Converting patch for edit buffer");
				std::shared_ptr<midikraft::DataFile> dataFile;
				try { dataFile = synth->patchFromPatchData(patch.payload, MidiProgramNumber::invalidProgram()); }
				catch (std::exception const& ex) { return error(ServiceErrorCode::PatchDataInvalid, ex.what()); }
				catch (...) { return error(ServiceErrorCode::PatchDataInvalid, "Adaptation rejected patch data"); }
				if (!dataFile) return error(ServiceErrorCode::PatchDataInvalid, "Adaptation rejected patch data");
				try {
					std::size_t consumed = 0;
					auto expectedType = std::stoi(patch.dataTypeId, &consumed);
					if (consumed != patch.dataTypeId.size() || expectedType != dataFile->dataTypeID())
						return error(ServiceErrorCode::PatchDataInvalid, "Patch data type is not accepted by the adaptation");
				}
				catch (...) { return error(ServiceErrorCode::PatchDataInvalid, "Patch data type is invalid"); }
				std::vector<juce::MidiMessage> messages;
				try { messages = editBuffer->patchToSysex(dataFile); }
				catch (std::exception const& ex) { return error(ServiceErrorCode::AdaptationError, ex.what()); }
				catch (...) { return error(ServiceErrorCode::AdaptationError, "Adaptation failed while creating edit-buffer messages"); }
				if (messages.empty()) return error(ServiceErrorCode::AdaptationError, "Adaptation produced no edit-buffer messages");
				if (cancelled.load()) return error(ServiceErrorCode::TransferCancelled, "Transfer cancelled");
				auto output = device->midiOutput();
				if (output.identifier.isEmpty()) return error(ServiceErrorCode::SynthOffline, "Synth has no configured MIDI output", true);
				progress(0.6, "Opening MIDI output");
				if (!midikraft::MidiController::instance()->enableMidiOutput(output))
					return error(ServiceErrorCode::MidiPortBusy, "MIDI output could not be opened", true);
				if (cancelled.load()) return error(ServiceErrorCode::TransferCancelled, "Transfer cancelled");
				progress(0.8, "Sending edit-buffer messages");
				try { synth->sendBlockOfMessagesToSynth(output, messages); }
				catch (std::exception const& ex) { return error(ServiceErrorCode::MidiPortBusy, ex.what(), true); }
				catch (...) { return error(ServiceErrorCode::MidiPortBusy, "MIDI send failed", true); }
				progress(1.0, "Edit-buffer messages sent");
				return ServiceResult<bool>::success(true);
			});
		}
		catch (std::exception const& ex) { return error(ServiceErrorCode::Unavailable, ex.what(), true); }
	}

	bool KnobKraftEngineBackend::openKnobKraft(NavigationTargetKind target, std::optional<std::string> const& targetId) {
		(void)target;
		(void)targetId;
		return onMessageThread([this] {
			if (!showApplication_) return false;
			showApplication_();
			return true;
		});
	}
}
