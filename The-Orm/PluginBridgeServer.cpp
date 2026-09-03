/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PluginBridgeServer.h"

#include <juce_core/juce_core.h>

namespace knobkraft::recall {
	using namespace midikraft::session;

	namespace {
		class JucePrimaryServerLease final : public PrimaryServerLease {
		public:
			bool tryAcquire() override { return lock_.enter(0); }
			void release() override { lock_.exit(); }
		private:
			juce::InterProcessLock lock_ { "KnobKraftOrm.Recall.PrimaryServer.v1" };
		};
	}

	std::filesystem::path recallDiscoveryFilePath() {
		return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
			.getChildFile("KnobKraftOrm").getChildFile("recall-session-v1.json")
			.getFullPathName().toStdString();
	}

	PluginBridgeServer::PluginBridgeServer(SessionService& service, std::shared_ptr<DiscoveryFile> discoveryFile,
		std::unique_ptr<PrimaryServerLease> primaryLease)
		: primaryLease_(primaryLease ? std::move(primaryLease) : std::make_unique<JucePrimaryServerLease>()) {
		if (!discoveryFile) discoveryFile = std::make_shared<DiscoveryFile>(recallDiscoveryFilePath());
		SessionIpcServerConfig config;
		config.discoveryFile = std::move(discoveryFile);
		server_ = std::make_unique<SessionIpcServer>(service, std::move(config));
	}

	PluginBridgeServer::~PluginBridgeServer() { stop(); }

	ServiceResult<bool> PluginBridgeServer::start() {
		if (server_->isRunning()) return ServiceResult<bool>::success(true);
		if (!primaryLease_->tryAcquire())
			return ServiceResult<bool>::failure({ ServiceErrorCode::Unavailable, "Another KnobKraft process owns the Recall bridge", true });
		ownsPrimaryLease_ = true;
		auto started = server_->start();
		if (!started) {
			primaryLease_->release();
			ownsPrimaryLease_ = false;
			return ServiceResult<bool>::failure(started.error());
		}
		return ServiceResult<bool>::success(true);
	}

	void PluginBridgeServer::stop() {
		if (server_) server_->stop();
		if (ownsPrimaryLease_) {
			primaryLease_->release();
			ownsPrimaryLease_ = false;
		}
	}

	bool PluginBridgeServer::isPrimary() const noexcept { return ownsPrimaryLease_; }
	bool PluginBridgeServer::isRunning() const noexcept { return server_ && server_->isRunning(); }
}
