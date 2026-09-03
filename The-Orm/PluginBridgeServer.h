/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "SessionTransport.h"

#include <filesystem>
#include <memory>

namespace knobkraft::recall {

	class PrimaryServerLease {
	public:
		virtual ~PrimaryServerLease() = default;
		virtual bool tryAcquire() = 0;
		virtual void release() = 0;
	};

	[[nodiscard]] std::filesystem::path recallDiscoveryFilePath();

	class PluginBridgeServer {
	public:
		PluginBridgeServer(midikraft::session::SessionService& service,
			std::shared_ptr<midikraft::session::DiscoveryFile> discoveryFile = {},
			std::unique_ptr<PrimaryServerLease> primaryLease = {});
		~PluginBridgeServer();

		[[nodiscard]] midikraft::session::ServiceResult<bool> start();
		void stop();
		[[nodiscard]] bool isPrimary() const noexcept;
		[[nodiscard]] bool isRunning() const noexcept;

	private:
		std::unique_ptr<PrimaryServerLease> primaryLease_;
		std::unique_ptr<midikraft::session::SessionIpcServer> server_;
		bool ownsPrimaryLease_ = false;
	};
}
