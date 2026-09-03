/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "ConfiguredSynthInstance.h"
#include "SessionService.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace knobkraft::recall {

	struct EnginePatch {
		std::string patchId;
		std::string adaptationId;
		std::string dataTypeId;
		std::string name;
		std::vector<std::uint8_t> payload;
		std::optional<midikraft::session::PatchProvenance> source;
	};

	struct PatchSearchPage {
		std::vector<EnginePatch> patches;
		bool hasMore = false;
	};

	// The application backend is deliberately narrower than the wire service.
	// Production implements it with the registry/database/MIDI stack; tests use
	// an in-memory implementation and never open a user database or MIDI port.
	class EngineSessionBackend {
	public:
		using Progress = std::function<void(double, std::string)>;
		virtual ~EngineSessionBackend() = default;

		virtual std::vector<midikraft::session::ConfiguredSynthInstance> configuredSynths() = 0;
		virtual PatchSearchPage searchPatches(std::string const& query,
			std::optional<std::string> const& adaptationId, std::size_t offset, std::size_t limit) = 0;
		virtual std::optional<EnginePatch> getPatch(std::string const& patchId) = 0;
		virtual midikraft::session::ServiceResult<bool> sendToEditBuffer(
			std::string const& configuredSynthInstanceId,
			midikraft::session::SessionPatch const& patch,
			std::atomic_bool const& cancelled,
			Progress progress) = 0;
		virtual bool openKnobKraft(midikraft::session::NavigationTargetKind target,
			std::optional<std::string> const& targetId) = 0;
	};

	struct SessionServiceAdapterConfig {
		std::function<std::int64_t()> nowUnixMillis;
		std::function<std::string()> transferIdGenerator;
		std::function<void(std::string const&)> auditLog;
		std::size_t maximumPageSize = 200;
	};

	class SessionServiceAdapter final : public midikraft::session::SessionService {
	public:
		explicit SessionServiceAdapter(EngineSessionBackend& backend, SessionServiceAdapterConfig config = {});
		~SessionServiceAdapter() override;

		SessionServiceAdapter(SessionServiceAdapter const&) = delete;
		SessionServiceAdapter& operator=(SessionServiceAdapter const&) = delete;

		midikraft::session::ServiceResult<midikraft::session::ServiceResponse<midikraft::session::ServerInfo>> getServerInfo(midikraft::session::RequestContext const& request) override;
		midikraft::session::ServiceResult<midikraft::session::ServiceResponse<midikraft::session::PagedItems<midikraft::session::SessionSynthInfo>>> listConfiguredSynthInstances(midikraft::session::ListSynthsRequest const& request) override;
		midikraft::session::ServiceResult<midikraft::session::ServiceResponse<midikraft::session::SessionSynthInfo>> getConfiguredSynthInstance(midikraft::session::GetSynthRequest const& request) override;
		midikraft::session::ServiceResult<midikraft::session::ServiceResponse<midikraft::session::PagedItems<midikraft::session::PatchSummary>>> searchPatches(midikraft::session::SearchPatchesRequest const& request) override;
		midikraft::session::ServiceResult<midikraft::session::ServiceResponse<midikraft::session::SessionPatch>> getPatch(midikraft::session::GetPatchRequest const& request) override;
		midikraft::session::ServiceResult<midikraft::session::ServiceResponse<midikraft::session::TransferRecord>> applyToEditBuffer(midikraft::session::ApplyToEditBufferRequest const& request) override;
		midikraft::session::ServiceResult<midikraft::session::ServiceResponse<midikraft::session::TransferRecord>> getTransferStatus(midikraft::session::GetTransferStatusRequest const& request) override;
		midikraft::session::ServiceResult<midikraft::session::ServiceResponse<midikraft::session::TransferRecord>> cancelTransfer(midikraft::session::CancelTransferRequest const& request) override;
		midikraft::session::ServiceResult<midikraft::session::ServiceResponse<midikraft::session::NavigationResult>> openKnobKraft(midikraft::session::OpenKnobKraftRequest const& request) override;
		midikraft::session::ServiceResult<midikraft::session::ServiceResponse<midikraft::session::SessionSnapshot>> publishSession(midikraft::session::PublishSessionRequest const& request) override;
		midikraft::session::ServiceResult<midikraft::session::ServiceResponse<midikraft::session::SessionSnapshot>> disconnectSession(midikraft::session::DisconnectSessionRequest const& request) override;
		midikraft::session::SessionSnapshot currentSnapshot() const override;
		midikraft::session::ObserverId subscribe(midikraft::session::SessionObserver observer, bool emitCurrent = true) override;
		void unsubscribe(midikraft::session::ObserverId observerId) override;

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};

}
