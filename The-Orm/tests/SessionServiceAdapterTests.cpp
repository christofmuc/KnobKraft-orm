#include "PluginBridgeServer.h"
#include "SessionCodecs.h"
#include "SessionServiceAdapter.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;
using namespace midikraft::session;
using namespace knobkraft::recall;

namespace {
	int failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::cerr << "CHECK failed at line " << __LINE__ << ": " #condition "\n"; ++failures; } } while (false)

	RequestContext context(std::string id) { return { std::move(id), "client", "plugin", std::nullopt }; }

	SessionPatch patch(std::string adaptation = "Matrix") {
		SessionPatch result;
		result.adaptationId = std::move(adaptation);
		result.dataTypeId = "1";
		result.name = "Warm Bass";
		result.payload = { 0xf0, 0x10, 0x01, 0xf7 };
		auto fingerprint = SessionPatchCodec::fingerprint(result);
		CHECK(fingerprint);
		if (fingerprint) result.fingerprint = fingerprint.value();
		return result;
	}

	ConfiguredSynthInstance synth(std::string id, bool online = true, bool editBuffer = true) {
		ConfiguredSynthInstance result;
		result.instanceId = std::move(id);
		result.displayName = result.instanceId;
		result.adaptationId = "Matrix";
		result.online = online;
		result.capabilities.editBuffer = editBuffer;
		return result;
	}

	class FakeBackend final : public EngineSessionBackend {
	public:
		std::vector<ConfiguredSynthInstance> synths { synth("matrix") };
		EnginePatch stored { "Matrix:abc", "Matrix", "1", "Warm Bass", { 0xf0, 0x10, 0x01, 0xf7 }, PatchProvenance { "abc", 0, 7 } };
		std::optional<ServiceError> sendError;
		std::atomic_int active { 0 };
		std::atomic_int maximumActive { 0 };
		std::atomic_int sends { 0 };
		std::chrono::milliseconds sendDuration { 30 };

		std::vector<ConfiguredSynthInstance> configuredSynths() override { return synths; }
		PatchSearchPage searchPatches(std::string const& query, std::optional<std::string> const& adaptation,
			std::size_t offset, std::size_t limit) override {
			PatchSearchPage page;
			if (offset == 0 && limit && (!adaptation || *adaptation == stored.adaptationId)
				&& (query.empty() || stored.name.find(query) != std::string::npos)) page.patches.push_back(stored);
			return page;
		}
		std::optional<EnginePatch> getPatch(std::string const& id) override { return id == stored.patchId ? std::optional(stored) : std::nullopt; }
		ServiceResult<bool> sendToEditBuffer(std::string const&, SessionPatch const&, std::atomic_bool const& cancelled,
			Progress progress) override {
			++sends;
			auto now = ++active;
			maximumActive.store(std::max(maximumActive.load(), now));
			progress(0.5, "fake send");
			auto until = std::chrono::steady_clock::now() + sendDuration;
			while (std::chrono::steady_clock::now() < until && !cancelled.load()) std::this_thread::sleep_for(2ms);
			--active;
			if (cancelled.load()) return ServiceResult<bool>::failure({ ServiceErrorCode::TransferCancelled, "cancelled", false });
			if (sendError) return ServiceResult<bool>::failure(*sendError);
			return ServiceResult<bool>::success(true);
		}
		bool openKnobKraft(NavigationTargetKind, std::optional<std::string> const&) override { return true; }
	};

	TransferRecord waitFor(SessionServiceAdapter& service, std::string const& transferId) {
		for (int attempt = 0; attempt < 500; ++attempt) {
			auto status = service.getTransferStatus({ context("status-" + std::to_string(attempt)), transferId });
			CHECK(status);
			if (status) {
				auto state = status.value().value.status.state;
				if (state == TransferState::Succeeded || state == TransferState::Failed || state == TransferState::Cancelled) return status.value().value;
			}
			std::this_thread::sleep_for(2ms);
		}
		CHECK(false);
		return {};
	}

	ApplyToEditBufferRequest apply(std::string requestId, std::string synthId = "matrix", std::string adaptation = "Matrix") {
		return { context(std::move(requestId)), std::move(synthId), adaptation, patch(std::move(adaptation)) };
	}

	void listsAndRetrievesDatabaseProjection() {
		FakeBackend backend;
		SessionServiceAdapter service(backend);
		auto listed = service.listConfiguredSynthInstances({ context("list"), {} });
		CHECK(listed && listed.value().value.items.size() == 1);
		auto searched = service.searchPatches({ context("search"), "Warm", "Matrix", {} });
		CHECK(searched && searched.value().value.items.size() == 1);
		auto retrieved = service.getPatch({ context("get"), "Matrix:abc" });
		CHECK(retrieved && retrieved.value().value.name == "Warm Bass");
		CHECK(retrieved && !retrieved.value().value.fingerprint.empty());
	}

	void validatesBeforeQueueing() {
		FakeBackend backend;
		SessionServiceAdapter service(backend);
		auto incompatible = service.applyToEditBuffer(apply("bad-adaptation", "matrix", "Other"));
		CHECK(!incompatible && incompatible.error().code == ServiceErrorCode::PatchIncompatible);
		backend.synths[0].online = false;
		auto offline = service.applyToEditBuffer(apply("offline"));
		CHECK(!offline && offline.error().code == ServiceErrorCode::SynthOffline);
		backend.synths[0] = synth("matrix", true, false);
		auto missingCapability = service.applyToEditBuffer(apply("no-edit"));
		CHECK(!missingCapability && missingCapability.error().code == ServiceErrorCode::AdaptationError);
		backend.synths[0] = synth("matrix");
		auto corrupt = apply("corrupt");
		corrupt.patch.payload.push_back(1);
		auto invalid = service.applyToEditBuffer(corrupt);
		CHECK(!invalid && invalid.error().code == ServiceErrorCode::PatchDataInvalid);
	}

	void serializesAndDeduplicatesPerDevice() {
		FakeBackend backend;
		backend.sendDuration = 80ms;
		SessionServiceAdapter service(backend);
		auto first = service.applyToEditBuffer(apply("same-request"));
		auto duplicate = service.applyToEditBuffer(apply("same-request"));
		auto second = service.applyToEditBuffer(apply("second-request"));
		CHECK(first && duplicate && second);
		CHECK(first.value().value.status.transferId == duplicate.value().value.status.transferId);
		waitFor(service, first.value().value.status.transferId);
		waitFor(service, second.value().value.status.transferId);
		CHECK(backend.sends.load() == 2);
		CHECK(backend.maximumActive.load() == 1);
	}

	void reportsBusyAndCancellation() {
		FakeBackend backend;
		backend.sendError = ServiceError { ServiceErrorCode::MidiPortBusy, "busy", true };
		SessionServiceAdapter service(backend);
		auto busy = service.applyToEditBuffer(apply("busy"));
		CHECK(busy);
		auto failed = waitFor(service, busy.value().value.status.transferId);
		CHECK(failed.status.state == TransferState::Failed);
		CHECK(failed.error && failed.error->code == ServiceErrorCode::MidiPortBusy);

		backend.sendError.reset();
		backend.sendDuration = 200ms;
		auto running = service.applyToEditBuffer(apply("running"));
		auto queued = service.applyToEditBuffer(apply("queued"));
		CHECK(running && queued);
		auto cancelled = service.cancelTransfer({ context("cancel"), queued.value().value.status.transferId });
		CHECK(cancelled);
		CHECK(waitFor(service, queued.value().value.status.transferId).status.state == TransferState::Cancelled);
	}

	class SharedLease final : public PrimaryServerLease {
	public:
		explicit SharedLease(std::shared_ptr<std::atomic_bool> held) : held_(std::move(held)) {}
		bool tryAcquire() override { bool expected = false; acquired_ = held_->compare_exchange_strong(expected, true); return acquired_; }
		void release() override { if (acquired_) held_->store(false); acquired_ = false; }
	private:
		std::shared_ptr<std::atomic_bool> held_;
		bool acquired_ = false;
	};

	void secondaryServerCannotReplaceDiscovery() {
		FakeBackend backend;
		SessionServiceAdapter service(backend);
		auto directory = std::filesystem::temp_directory_path() / ("knobkraft-wp05-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		auto discovery = std::make_shared<DiscoveryFile>(directory / "recall.json");
		auto held = std::make_shared<std::atomic_bool>(false);
		PluginBridgeServer primary(service, discovery, std::make_unique<SharedLease>(held));
		CHECK(primary.start());
		auto before = discovery->read(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(), 10s);
		CHECK(before);
		PluginBridgeServer secondary(service, discovery, std::make_unique<SharedLease>(held));
		auto rejected = secondary.start();
		CHECK(!rejected && rejected.error().code == ServiceErrorCode::Unavailable);
		auto after = discovery->read(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(), 10s);
		CHECK(after && before && after.value() == before.value());
		primary.stop();
		std::error_code ignored;
		std::filesystem::remove_all(directory, ignored);
	}
}

int main() {
	listsAndRetrievesDatabaseProjection();
	validatesBeforeQueueing();
	serializesAndDeduplicatesPerDevice();
	reportsBusyAndCancellation();
	secondaryServerCannotReplaceDiscovery();
	if (failures) std::cerr << failures << " WP-05 checks failed\n";
	return failures ? 1 : 0;
}
