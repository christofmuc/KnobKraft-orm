/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "FakeSessionService.h"
#include "PluginSessionsModel.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
	using namespace knobkraft::sessions;
	using namespace midikraft::session;

	void check(bool condition, std::string const& message) {
		if (!condition) throw std::runtime_error(message);
	}

	PluginSessionState session(std::string clientId, std::string pluginId, std::string name, std::int64_t lastSeen,
		std::optional<std::string> synthId = "synth-matrix") {
		return { std::move(clientId), std::move(pluginId), std::move(name), "Ableton Live",
			{ std::move(synthId), "Oberheim Matrix 1000" }, "Warm Bass", "sha256:fixture", lastSeen };
	}

	SessionSynthInfo synth(std::string id = "synth-matrix", bool online = true) {
		return { std::move(id), "Studio Matrix-1000", "Oberheim Matrix 1000", online, { true, true, false, false } };
	}

	TransferRecord transfer(std::string clientId, std::string pluginId, std::string transferId, TransferState state,
		std::int64_t updatedAt, std::string detail = {}) {
		TransferRecord record;
		record.status.transferId = std::move(transferId);
		record.status.requestId = "request";
		record.status.state = state;
		record.status.progress = state == TransferState::Sending ? std::optional<double>(0.5) : std::nullopt;
		record.status.detail = std::move(detail);
		record.clientId = std::move(clientId);
		record.pluginInstanceId = std::move(pluginId);
		record.pluginInstanceName = "Matrix Bass";
		record.configuredSynthInstanceId = "synth-matrix";
		record.patchName = "Warm Bass";
		record.patchFingerprint = "sha256:fixture";
		record.updatedAtUnixMillis = updatedAt;
		if (state == TransferState::Failed)
			record.error = ServiceError { ServiceErrorCode::AdaptationError, record.status.detail, false };
		return record;
	}

	RequestContext context(std::string requestId, std::string clientId = "client-a", std::string pluginId = "plugin-a") {
		return { std::move(requestId), std::move(clientId), std::move(pluginId), 1'700'000'005'000 };
	}

	void countsAndEditorLifetimeFollowProcessorSessions() {
		PluginSessionsModel model;
		SessionSnapshot snapshot;
		model.reduce(snapshot, { synth() }, 1000);
		check(model.view().pillMode == PillMode::Disconnected, "empty snapshot must be disconnected");

		snapshot.sessions.push_back(session("client-a", "plugin-a", "Matrix Bass", 1000));
		model.reduce(snapshot, { synth() }, 1000);
		check(model.view().rows.size() == 1, "connected processor must appear");
		check(model.view().pillText == "1 plugin session", "one session count is wrong");

		// An editor close emits no service event. Reducing the same processor
		// heartbeat must therefore retain the session.
		model.reduce(snapshot, { synth() }, 1500);
		check(model.view().rows.size() == 1, "closing an editor must not remove its processor session");

		snapshot.sessions.push_back(session("client-b", "plugin-b", "Prophet Pad", 1500));
		model.reduce(snapshot, { synth() }, 1500);
		check(model.view().rows.size() == 2, "second connection must update count");

		snapshot.sessions.erase(snapshot.sessions.begin());
		model.reduce(snapshot, { synth() }, 1500);
		check(model.view().rows.size() == 1, "disconnect must remove exactly one session");
	}

	void staleSessionsExpire() {
		PluginSessionsModel model(std::chrono::milliseconds(3000));
		SessionSnapshot snapshot { 1, { session("client-a", "plugin-a", "Matrix Bass", 1000) }, {} };
		model.reduce(snapshot, { synth() }, 4000);
		check(model.view().rows.size() == 1, "session is valid at the timeout boundary");
		model.reduce(snapshot, { synth() }, 4001);
		check(model.view().rows.empty(), "stale heartbeat must expire session");
	}

	void progressErrorsAndAcknowledgementReducePredictably() {
		PluginSessionsModel model;
		SessionSnapshot snapshot { 1, { session("client-a", "plugin-a", "Matrix Bass", 1000) },
			{ transfer("client-a", "plugin-a", "transfer-a", TransferState::Sending, 1000, "Sending bytes") } };
		model.reduce(snapshot, { synth() }, 1000);
		check(model.view().pillMode == PillMode::ActiveTransfer, "sending transfer must drive active pill");
		check(model.view().rows[0].activeOperation->progress == 0.5, "progress must be retained");

		snapshot.transfers = { transfer("client-a", "plugin-a", "transfer-a", TransferState::Failed, 1100, "MIDI port busy") };
		model.reduce(snapshot, { synth() }, 1100);
		check(model.view().pillMode == PillMode::Attention, "failed transfer must request attention");
		check(model.view().rows[0].lastResult->detail == "MIDI port busy", "failure detail must be visible");

		snapshot.transfers.clear();
		model.reduce(snapshot, { synth() }, 1200);
		check(model.view().rows[0].attention, "errors must persist when compact snapshots omit old transfers");
		model.acknowledgeAttention("plugin-a");
		check(!model.view().rows[0].attention, "acknowledgement must clear transfer attention");

		snapshot.transfers = { transfer("client-a", "plugin-a", "transfer-b", TransferState::Succeeded, 1300, "Sent") };
		model.reduce(snapshot, { synth() }, 1300);
		check(model.view().rows[0].lastResult->state == TransferState::Succeeded, "later result must replace failure");
	}

	void bindingHealthProducesResolveState() {
		PluginSessionsModel model;
		SessionSnapshot snapshot { 1, { session("client-a", "plugin-a", "Matrix Bass", 1000) }, {} };
		model.reduce(snapshot, { synth("synth-matrix", false) }, 1000);
		check(model.view().rows[0].synthState == SynthState::Offline, "offline synth state is wrong");
		check(model.view().rows[0].attention, "offline synth must request attention");
		model.reduce(snapshot, {}, 1000);
		check(model.view().rows[0].synthState == SynthState::Missing, "missing binding must be explicit");
	}

	void fakeServiceCancellationPropagatesThroughController() {
		FakeSessionService service;
		std::int64_t now = service.nowUnixMillis();
		std::uint64_t request = 1;
		PluginSessionsController controller([&]() { return now; }, [&]() { return "widget-" + std::to_string(request++); });
		controller.setService(&service);
		controller.setSynths({ synth() });

		auto published = service.publishSession({ context("publish"), "Matrix Bass", "Ableton Live",
			{ "synth-matrix", "Oberheim Matrix 1000" }, "Warm Bass", "sha256:fixture" });
		check(static_cast<bool>(published), "fake session publish failed");
		auto patch = service.getPatch({ context("get-patch"), "patch-warm-bass" });
		check(static_cast<bool>(patch), "fake patch lookup failed");
		auto applied = service.applyToEditBuffer({ context("apply"), "synth-matrix", "Oberheim Matrix 1000", patch.value().value });
		check(static_cast<bool>(applied), "fake transfer start failed");
		service.advanceTransfers();
		check(controller.view().rows[0].activeOperation.has_value(), "fake transfer progress was not observed");

		auto cancelled = controller.cancel("plugin-a", applied.value().value.status.transferId);
		check(cancelled.accepted, "drawer cancellation did not reach service");
		auto status = service.getTransferStatus({ context("status"), applied.value().value.status.transferId });
		check(status.value().value.status.state == TransferState::Cancelled, "service did not cancel transfer");
	}

	void fakeServiceFailureRemainsVisible() {
		FakeSessionService service;
		auto const now = service.nowUnixMillis();
		PluginSessionsController controller([&]() { return now; });
		controller.setService(&service);
		controller.setSynths({ synth() });
		check(static_cast<bool>(service.publishSession({ context("publish-failure"), "Matrix Bass", std::nullopt,
			{ "synth-matrix", "Oberheim Matrix 1000" }, "Warm Bass", "sha256:fixture" })), "failure session publish failed");
		auto patch = service.getPatch({ context("get-failure-patch"), "patch-warm-bass" });
		service.setTransferBehavior("apply-failure", { TransferState::Preparing,
			{ ServiceErrorCode::MidiPortBusy, "MIDI port busy", true } });
		check(static_cast<bool>(service.applyToEditBuffer({ context("apply-failure"), "synth-matrix",
			"Oberheim Matrix 1000", patch.value().value })), "failure transfer start failed");
		service.advanceTransfers();
		check(controller.view().pillMode == PillMode::Attention, "fake service failure must drive attention pill");
		check(controller.view().rows[0].lastResult->detail == "MIDI port busy", "fake failure detail must remain visible");
	}

	void navigationIsAnInjectedIntent() {
		std::int64_t now = 1000;
		FakeSessionService service;
		service.setNowUnixMillis(now);
		PluginSessionsController controller([&]() { return now; });
		NavigationIntent received;
		bool called = false;
		controller.setNavigationHandler([&](NavigationIntent const& intent) { received = intent; called = true; });
		controller.setService(&service);
		controller.setSynths({ synth() });
		auto published = service.publishSession({ { "publish", "client-a", "plugin-a", 2000 }, "Matrix Bass", std::nullopt,
			{ "synth-matrix", "Oberheim Matrix 1000" }, "Warm Bass", "sha256:fixture" });
		check(static_cast<bool>(published), "navigation fixture publish failed");
		auto result = controller.navigate("plugin-a", NavigationTarget::Synth);
		check(result.accepted && called, "navigation handler was not called");
		check(received == NavigationIntent { NavigationTarget::Synth, "plugin-a", "synth-matrix" }, "navigation intent is wrong");
	}
}

int main() {
	try {
		countsAndEditorLifetimeFollowProcessorSessions();
		staleSessionsExpire();
		progressErrorsAndAcknowledgementReducePredictably();
		bindingHealthProducesResolveState();
		fakeServiceCancellationPropagatesThroughController();
		fakeServiceFailureRemainsVisible();
		navigationIsAnInjectedIntent();
		std::cout << "Plugin Sessions model tests passed\n";
		return 0;
	}
	catch (std::exception const& error) {
		std::cerr << "Plugin Sessions model tests failed: " << error.what() << '\n';
		return 1;
	}
}
