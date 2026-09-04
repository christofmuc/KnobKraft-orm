#include "doctest/doctest.h"

#include "BankDumpCapability.h"
#include "EditBufferCapability.h"
#include "GenericAdaptation.h"
#include "MidiHelpers.h"
#include "MidiController.h"
#include "PatchDatabase.h"
#include "Sysex.h"
#include "Virus.h"
#include "test_helpers.h"

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>
#include <pybind11/embed.h>

#include <filesystem>
#include <memory>
#include <vector>

namespace {

constexpr auto kMks50Name = "Roland MKS-50";
constexpr auto kMks50LegacyMd5 = "a726ccbd117b50eff32d618b1402fe79";

class ScopedMks50Database {
public:
	ScopedMks50Database()
		: path_(std::filesystem::temp_directory_path() /
			("mks50_legacy_" + juce::Uuid().toString().toStdString() + ".db3")) {}

	~ScopedMks50Database() {
		std::error_code ec;
		std::filesystem::remove(path_, ec);
	}

	std::filesystem::path const& path() const { return path_; }

private:
	std::filesystem::path path_;
};

void createLegacyMks50Database(std::filesystem::path const& path, std::vector<uint8> const& payload) {
	SQLite::Database db(path.string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
	db.exec("CREATE TABLE schema_version (number INTEGER)");
	db.exec("INSERT INTO schema_version (number) VALUES (13)");
	db.exec("CREATE TABLE patches (synth TEXT NOT NULL, md5 TEXT NOT NULL, name TEXT, type INTEGER, data BLOB, favorite INTEGER, hidden INTEGER, sourceID TEXT, sourceName TEXT, sourceInfo TEXT, midiBankNo INTEGER, midiProgramNo INTEGER, categories INTEGER, categoryUserDecision INTEGER, comment TEXT, PRIMARY KEY (synth, md5))");
	db.exec("CREATE TABLE imports (synth TEXT, name TEXT, id TEXT, date TEXT)");
	db.exec("CREATE TABLE lists(id TEXT PRIMARY KEY, name TEXT NOT NULL, synth TEXT, midi_bank_number INTEGER, last_synced INTEGER)");
	db.exec("CREATE TABLE patch_in_list(id TEXT NOT NULL, synth TEXT NOT NULL, md5 TEXT NOT NULL, order_num INTEGER NOT NULL)");
	db.exec("CREATE TABLE categories (bitIndex INTEGER UNIQUE, name TEXT, color TEXT, active INTEGER)");

	SQLite::Statement insertPatch(db,
		"INSERT INTO patches (synth, md5, name, type, data, favorite, hidden, sourceID, sourceName, sourceInfo, midiBankNo, midiProgramNo, categories, categoryUserDecision, comment) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
	insertPatch.bind(1, kMks50Name);
	insertPatch.bind(2, kMks50LegacyMd5);
	insertPatch.bind(3, "PolySynth1");
	insertPatch.bind(4, 0);
	insertPatch.bind(5, payload.data(), static_cast<int>(payload.size()));
	insertPatch.bind(6, 0);
	insertPatch.bind(7, 0);
	insertPatch.bind(8, "mks50-fixture-import");
	insertPatch.bind(9, "FACTORYA.SYX");
	insertPatch.bind(10, R"({"bulksource":true,"timestamp":"2021-01-01T12:00:00Z"})");
	insertPatch.bind(11, 0);
	insertPatch.bind(12, 0);
	insertPatch.bind(13, 0);
	insertPatch.bind(14, 0);
	insertPatch.bind(15, "");
	insertPatch.exec();

	SQLite::Statement insertImport(db, "INSERT INTO imports (synth, name, id, date) VALUES (?, ?, ?, ?)");
	insertImport.bind(1, kMks50Name);
	insertImport.bind(2, "FACTORYA.SYX");
	insertImport.bind(3, "mks50-fixture-import");
	insertImport.bind(4, "2021-01-01 12:00:00");
	insertImport.exec();
}

class BankSequenceSynth : public test_helpers::DummySynth, public midikraft::BankDumpCapability {
public:
	BankSequenceSynth() : DummySynth("Bank sequence test") {}

	HandshakeReply isMessagePartOfBankDump(const juce::MidiMessage& message) const override {
		if (message.getRawDataSize() == 0) {
			resetCalls++;
			return { false, {} };
		}
		return { true, {} };
	}

	FinishedReply bankDumpFinishedWithReply(std::vector<juce::MidiMessage> const& bankDump) const override {
		if (bankDump.empty()) {
			return { false, {} };
		}
		auto code = bankDump.back().getRawData()[1];
		if (code == 0x01) {
			return { true, {}, false };
		}
		return { code == 0x02, {} };
	}

	midikraft::TPatchVector patchesFromSysexBank(std::vector<juce::MidiMessage> const& messages) const override {
		parsedBankSize = messages.size();
		auto patch = std::make_shared<test_helpers::DummyPatch>();
		patch->setData({ 0x02 });
		return { patch };
	}

	mutable int resetCalls = 0;
	mutable size_t parsedBankSize = 0;
};

TEST_CASE("generic bank dump bridge and MKS-50 legacy records remain compatible") {
	pybind11::scoped_interpreter python;
	auto pythonSystem = pybind11::module_::import("sys");
	CHECK_FALSE(pybind11::hasattr(pythonSystem, "_knobkraft_adaptation_api_version"));
	auto adaptation = knobkraft::GenericAdaptation::fromBinaryCode(
		"bank_dump_bridge_test",
		R"(
def name():
    return "Bank dump bridge test"

def createDeviceDetectMessage(channel):
    return []

def channelIfValidDeviceResponse(message):
    return -1

def convertPatchesToBankDump(patches):
    return [[0xF0, 0x01, 0xF7], [0xF0, 0x02, 0xF7]]

def isPartOfBankDump(message):
    return False

def isBankDumpFinished(messages):
    return True, False, [[0xF0, 0x43, 0xF7]]

def extractPatchesFromAllBankMessages(messages):
    return []
)");

	REQUIRE(adaptation);
	CHECK(pythonSystem.attr("_knobkraft_adaptation_api_version").cast<int>() == 2);
	std::shared_ptr<midikraft::BankSendCapability> sendCapability;
	REQUIRE(adaptation->hasCapability(sendCapability));
	auto bankMessages = sendCapability->createBankMessages({});
	REQUIRE(bankMessages.size() == 2);
	CHECK(knobkraft::GenericAdaptation::messageToVector(bankMessages[0]) == std::vector<int>{ 0xf0, 0x01, 0xf7 });
	CHECK(knobkraft::GenericAdaptation::messageToVector(bankMessages[1]) == std::vector<int>{ 0xf0, 0x02, 0xf7 });

	std::shared_ptr<midikraft::BankDumpCapability> dumpCapability;
	REQUIRE(adaptation->hasCapability(dumpCapability));
	auto finished = dumpCapability->bankDumpFinishedWithReply({});
	CHECK(finished.isFinished);
	CHECK_FALSE(finished.wasSuccessful);
	REQUIRE(finished.handshakeReply.size() == 1);
	CHECK(knobkraft::GenericAdaptation::messageToVector(finished.handshakeReply[0]) == std::vector<int>{ 0xf0, 0x43, 0xf7 });

	auto adaptationsPath = (std::filesystem::path(KNOBKRAFT_TEST_SOURCE_DIR) / "adaptations").string();
	pybind11::module_::import("sys").attr("path").attr("insert")(0, adaptationsPath);
	auto genericMks50 = std::make_shared<knobkraft::GenericAdaptation>("Roland_MKS50");
	auto fixture = Sysex::loadSysex((std::filesystem::path(KNOBKRAFT_TEST_SOURCE_DIR) /
		"adaptations" / "testData" / "Roland_MKS50" / "FACTORYA.SYX").string());
	auto genericPatches = genericMks50->loadSysex(fixture);
	REQUIRE(genericPatches.size() == 64);

	// The retired native implementation stored only these 36 APR parameter bytes.
	// Before its removal, all 64 FACTORYA patches were cross-checked byte-for-byte,
	// including their names and fingerprints, against the Python adaptation.
	const std::vector<uint8> polySynthPayload = {
		0, 2, 2, 3, 3, 0, 2, 0, 0, 1, 1, 0, 0, 3, 110, 64, 77, 0,
		0, 98, 10, 12, 71, 0, 87, 46, 0, 127, 0, 122, 48, 52, 40, 1, 80, 2
	};
	const std::vector<uint8> polySynthApr = {
		240, 65, 53, 0, 35, 32, 1,
		0, 2, 2, 3, 3, 0, 2, 0, 0, 1, 1, 0, 0, 3, 110, 64, 77, 0,
		0, 98, 10, 12, 71, 0, 87, 46, 0, 127, 0, 122, 48, 52, 40, 1, 80, 2,
		15, 40, 37, 50, 18, 50, 39, 45, 33, 53, 247
	};
	const std::vector<uint8> jazzGuitarPayload = {
		0, 2, 2, 3, 0, 0, 2, 0, 0, 0, 0, 0, 0, 5, 39, 0, 43, 0,
		0, 107, 10, 0, 104, 0, 86, 42, 0, 127, 34, 104, 74, 0, 44, 5, 71, 2
	};
	const std::vector<uint8> jazzGuitarApr = {
		240, 65, 53, 0, 35, 32, 1,
		0, 2, 2, 3, 0, 0, 2, 0, 0, 0, 0, 0, 0, 5, 39, 0, 43, 0,
		0, 107, 10, 0, 104, 0, 86, 42, 0, 127, 34, 104, 74, 0, 44, 5, 71, 2,
		9, 26, 51, 51, 6, 46, 34, 45, 26, 43, 247
	};

	CHECK(genericPatches[0]->data() == polySynthApr);
	CHECK(genericMks50->nameForPatch(genericPatches[0]) == "PolySynth1");
	CHECK(genericMks50->calculateFingerprint(genericPatches[0]) == kMks50LegacyMd5);
	CHECK(std::vector<uint8>(polySynthApr.begin() + 7, polySynthApr.begin() + 43) == polySynthPayload);
	CHECK(genericPatches[1]->data() == jazzGuitarApr);
	CHECK(genericMks50->nameForPatch(genericPatches[1]) == "JazzGuitar");
	CHECK(genericMks50->calculateFingerprint(genericPatches[1]) == "16330f5f5cde81a95f359360420619fb");
	CHECK(std::vector<uint8>(jazzGuitarApr.begin() + 7, jazzGuitarApr.begin() + 43) == jazzGuitarPayload);

	ScopedMks50Database legacyDatabase;
	createLegacyMks50Database(legacyDatabase.path(), polySynthPayload);
	midikraft::PatchDatabase database(legacyDatabase.path().string(), midikraft::PatchDatabase::OpenMode::READ_WRITE);
	std::vector<midikraft::PatchHolder> loaded;
	REQUIRE(database.getSinglePatch(genericMks50, kMks50LegacyMd5, loaded));
	REQUIRE(loaded.size() == 1);
	auto const& restored = loaded.front();
	CHECK(restored.name() == "PolySynth1");
	CHECK(restored.md5() == kMks50LegacyMd5);
	CHECK(restored.bankNumber().toZeroBased() == 0);
	CHECK(restored.patchNumber().toZeroBasedDiscardingBank() == 0);
	CHECK(restored.patch()->data() == polySynthApr);

	std::shared_ptr<midikraft::EditBufferCapability> editBuffer;
	REQUIRE(genericMks50->hasCapability(editBuffer));
	auto exported = editBuffer->patchToSysex(restored.patch());
	REQUIRE(exported.size() == 1);
	CHECK(knobkraft::GenericAdaptation::messageToVector(exported.front()) ==
		std::vector<int>(polySynthApr.begin(), polySynthApr.end()));
}

TEST_CASE("Virus rejects truncated SysEx safely") {
	midikraft::Virus virus;
	auto headerOnly = MidiHelpers::sysexMessage({ 0x00, 0x20, 0x33, 0x01 });
	auto truncatedSingle = MidiHelpers::sysexMessage({ 0x00, 0x20, 0x33, 0x01, 0x10, 0x10, 0x01 });

	CHECK_FALSE(virus.isEditBufferDump({ headerOnly }));
	CHECK_FALSE(virus.isSingleProgramDump({ headerOnly }));
	CHECK_FALSE(virus.isEditBufferDump({ truncatedSingle }));
	CHECK_FALSE(virus.isSingleProgramDump({ truncatedSingle }));
	CHECK_FALSE(virus.bankDumpFinishedWithReply({ truncatedSingle }).isFinished);
	CHECK_FALSE(virus.channelIfValidDeviceResponse(headerOnly).isValid());
}

TEST_CASE("offline parsing clears a failed bank before a later valid bank") {
	BankSequenceSynth synth;
	auto failedBank = MidiHelpers::sysexMessage({ 0x01 });
	auto validBank = MidiHelpers::sysexMessage({ 0x02 });

	auto patches = synth.loadSysex({ failedBank, validBank });

	CHECK(midikraft::MidiController::isTimeoutMessage(midikraft::MidiController::makeTimeoutMessage()));
	REQUIRE(patches.size() == 1);
	CHECK(synth.parsedBankSize == 1);
	CHECK(synth.resetCalls == 2);
}

}
