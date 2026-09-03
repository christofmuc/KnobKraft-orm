/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "SessionServiceAdapter.h"

#include <functional>
#include <memory>

namespace midikraft {
	class PatchDatabase;
}

namespace knobkraft::recall {

	class KnobKraftEngineBackend final : public EngineSessionBackend {
	public:
		explicit KnobKraftEngineBackend(midikraft::PatchDatabase& database, std::function<void()> showApplication);

		std::vector<midikraft::session::ConfiguredSynthInstance> configuredSynths() override;
		PatchSearchPage searchPatches(std::string const& query,
			std::optional<std::string> const& adaptationId, std::size_t offset, std::size_t limit) override;
		std::optional<EnginePatch> getPatch(std::string const& patchId) override;
		midikraft::session::ServiceResult<bool> sendToEditBuffer(
			std::string const& configuredSynthInstanceId,
			midikraft::session::SessionPatch const& patch,
			std::atomic_bool const& cancelled,
			Progress progress) override;
		bool openKnobKraft(midikraft::session::NavigationTargetKind target,
			std::optional<std::string> const& targetId) override;

	private:
		midikraft::PatchDatabase& database_;
		std::function<void()> showApplication_;
	};
}
