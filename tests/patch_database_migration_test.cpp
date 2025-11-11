#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "../third_party/cereal/unittests/doctest.h"

#include "PatchDatabase.h"
#include "ImportList.h"
#include "Patch.h"
#include "HasBanksCapability.h"

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>

#include <filesystem>
#include <map>
#include <random>

namespace {

constexpr int kLegacySchemaVersion = 13;
constexpr auto kLegacySynth = "TestSynth";
constexpr auto kLegacyMd5 = "md5-aaa";
constexpr auto kLegacyImportId = "import-legacy-001";
constexpr auto kLegacyImportName = "Legacy Bulk Import";

enum ListType {
	NORMAL_LIST = 0,
	SYNTH_BANK = 1,
	USER_BANK = 2,
	IMPORT_LIST = 3
};

class ScopedTempFile {
public:
	explicit ScopedTempFile(std::filesystem::path path) : path_(std::move(path)) {}
	~ScopedTempFile() {
		std::error_code ec;
		std::filesystem::remove(path_, ec);
	}

	std::filesystem::path const& path() const { return path_; }

private:
	std::filesystem::path path_;
};

std::string makeRandomSuffix() {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<int> dist(0, 0xFFFFFF);
	return std::to_string(dist(gen));
}

ScopedTempFile makeTempDatabasePath() {
	auto base = std::filesystem::temp_directory_path();
	return ScopedTempFile(base / ("patch_database_migration_" + makeRandomSuffix() + ".db3"));
}

void createLegacyImportDatabase(const std::filesystem::path& dbPath) {
	std::error_code ec;
	std::filesystem::remove(dbPath, ec);
	SQLite::Database db(dbPath.string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

	db.exec("CREATE TABLE schema_version (number INTEGER)");
	SQLite::Statement insertVersion(db, "INSERT INTO schema_version (number) VALUES (?)");
	insertVersion.bind(1, kLegacySchemaVersion);
	insertVersion.exec();

	db.exec("CREATE TABLE patches (synth TEXT NOT NULL, md5 TEXT NOT NULL, name TEXT, type INTEGER, data BLOB, favorite INTEGER, hidden INTEGER, sourceID TEXT, sourceName TEXT, sourceInfo TEXT, midiBankNo INTEGER, midiProgramNo INTEGER, categories INTEGER, categoryUserDecision INTEGER, comment TEXT, PRIMARY KEY (synth, md5))");
	db.exec("CREATE TABLE imports (synth TEXT, name TEXT, id TEXT, date TEXT)");
	db.exec("CREATE TABLE lists(id TEXT PRIMARY KEY, name TEXT NOT NULL, synth TEXT, midi_bank_number INTEGER, last_synced INTEGER)");
	db.exec("CREATE TABLE patch_in_list(id TEXT NOT NULL, synth TEXT NOT NULL, md5 TEXT NOT NULL, order_num INTEGER NOT NULL)");
	db.exec("CREATE TABLE categories (bitIndex INTEGER UNIQUE, name TEXT, color TEXT, active INTEGER)");

	SQLite::Statement insertPatch(db,
		"INSERT INTO patches (synth, md5, name, type, data, favorite, hidden, sourceID, sourceName, sourceInfo, midiBankNo, midiProgramNo, categories, categoryUserDecision, comment) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
	insertPatch.bind(1, kLegacySynth);
	insertPatch.bind(2, kLegacyMd5);
	insertPatch.bind(3, "Bass 01");
	insertPatch.bind(4, 0);
	constexpr unsigned char kPatchBytes[] = { 0x01, 0x02, 0x03, 0x04 };
	insertPatch.bind(5, kPatchBytes, static_cast<int>(std::size(kPatchBytes)));
	insertPatch.bind(6, 0);  // favorite
	insertPatch.bind(7, 0);  // hidden
	insertPatch.bind(8, kLegacyImportId);
	insertPatch.bind(9, kLegacyImportName);
	insertPatch.bind(10, R"({"bulksource":true,"timestamp":"2024-01-01T12:00:00Z"})");
	insertPatch.bind(11, 0); // midiBankNo
	insertPatch.bind(12, 0); // midiProgramNo
	insertPatch.bind(13, 0); // categories
	insertPatch.bind(14, 0); // categoryUserDecision
	insertPatch.bind(15, ""); // comment
	insertPatch.exec();

	SQLite::Statement insertImport(db, "INSERT INTO imports (synth, name, id, date) VALUES (?, ?, ?, ?)");
	insertImport.bind(1, kLegacySynth);
	insertImport.bind(2, kLegacyImportName);
	insertImport.bind(3, kLegacyImportId);
	insertImport.bind(4, "2024-01-01 12:00:00");
	insertImport.exec();
}

class DummyPatch : public midikraft::Patch {
public:
	DummyPatch() : midikraft::Patch(0) {}
	MidiProgramNumber patchNumber() const override {
		return MidiProgramNumber::invalidProgram();
	}
};

class DummySynth : public midikraft::Synth, public midikraft::HasBanksCapability {
public:
	explicit DummySynth(std::string name) : name_(std::move(name)) {}

	std::shared_ptr<midikraft::DataFile> patchFromPatchData(const Synth::PatchData& data, MidiProgramNumber) const override {
		auto patch = std::make_shared<DummyPatch>();
		patch->setData(data);
		return patch;
	}

	bool isOwnSysex(juce::MidiMessage const&) const override { return true; }

	std::string getName() const override { return name_; }

	// HasBanksCapability
	int numberOfBanks() const override { return 1; }
	int numberOfPatches() const override { return 128; }
	std::string friendlyBankName(MidiBankNumber) const override { return "Dummy Bank"; }
	std::vector<juce::MidiMessage> bankSelectMessages(MidiBankNumber) const override { return {}; }

private:
	std::string name_;
};

void expectLegacyImports(SQLite::Database& db) {
	SQLite::Statement versionQuery(db, "SELECT number FROM schema_version");
	REQUIRE(versionQuery.executeStep());
	CHECK(versionQuery.getColumn(0).getInt() == kLegacySchemaVersion);

	SQLite::Statement importQuery(db, "SELECT name, id, date FROM imports WHERE synth = :SYN");
	importQuery.bind(":SYN", kLegacySynth);
	REQUIRE(importQuery.executeStep());
	CHECK(std::string(importQuery.getColumn(0).getText()) == kLegacyImportName);
	CHECK(std::string(importQuery.getColumn(1).getText()) == kLegacyImportId);
	CHECK(std::string(importQuery.getColumn(2).getText()).find("2024") != std::string::npos);

	SQLite::Statement patchQuery(db, "SELECT md5, sourceID FROM patches WHERE synth = :SYN");
	patchQuery.bind(":SYN", kLegacySynth);
	REQUIRE(patchQuery.executeStep());
	CHECK(std::string(patchQuery.getColumn(0).getText()) == kLegacyMd5);
	CHECK(std::string(patchQuery.getColumn(1).getText()) == kLegacyImportId);
}

} // namespace

TEST_CASE("legacy schema exposes imports before migration") {
	auto tmp = makeTempDatabasePath();
	createLegacyImportDatabase(tmp.path());

	SQLite::Database legacy(tmp.path().string(), SQLite::OPEN_READONLY);
	expectLegacyImports(legacy);
}

TEST_CASE("legacy imports migrate into list records and APIs work") {
	auto tmp = makeTempDatabasePath();
	createLegacyImportDatabase(tmp.path());

	{
		midikraft::PatchDatabase migrator(tmp.path().string(), midikraft::PatchDatabase::OpenMode::READ_WRITE);
	}

	{
		SQLite::Database verify(tmp.path().string(), SQLite::OPEN_READWRITE);

		SQLite::Statement importListQuery(verify, "SELECT list_type, synth, last_synced FROM lists WHERE id = :ID");
		importListQuery.bind(":ID", kLegacyImportId);
		REQUIRE(importListQuery.executeStep());
		CHECK(importListQuery.getColumn("list_type").getInt() == ListType::IMPORT_LIST);
		CHECK(std::string(importListQuery.getColumn("synth").getText()) == kLegacySynth);
		CHECK(importListQuery.getColumn("last_synced").getInt64() > 0);

		SQLite::Statement pilQuery(verify, "SELECT order_num FROM patch_in_list WHERE id = :ID AND md5 = :MD5");
		pilQuery.bind(":ID", kLegacyImportId);
		pilQuery.bind(":MD5", kLegacyMd5);
		REQUIRE(pilQuery.executeStep());
		CHECK(pilQuery.getColumn(0).getInt() == 0);
	}

	midikraft::PatchDatabase db(tmp.path().string(), midikraft::PatchDatabase::OpenMode::READ_WRITE);
	auto dummySynth = std::make_shared<DummySynth>(kLegacySynth);

	auto imports = db.getImportsList(dummySynth.get());
	REQUIRE(imports.size() == 1);
	CHECK(imports.front().id == kLegacyImportId);
	CHECK(imports.front().name == kLegacyImportName);
	CHECK(imports.front().countPatches == 1);

	std::map<std::string, std::weak_ptr<midikraft::Synth>> synthMap;
	synthMap[kLegacySynth] = dummySynth;
	auto list = db.getPatchList({ kLegacyImportId, kLegacyImportName }, synthMap);
	REQUIRE(list);
	auto importList = std::dynamic_pointer_cast<midikraft::ImportList>(list);
	REQUIRE(importList);
	auto patchesInList = importList->patches();
	REQUIRE(patchesInList.size() == 1);
	CHECK(patchesInList.front().synth()->getName() == kLegacySynth);
	CHECK(patchesInList.front().name() == "Bass 01");
	CHECK(patchesInList.front().synth()->getName() == kLegacySynth);

	db.removePatchFromList(kLegacyImportId, kLegacySynth, kLegacyMd5, 0);

	{
		SQLite::Database verify(tmp.path().string(), SQLite::OPEN_READONLY);
		SQLite::Statement remainingEntries(verify, "SELECT COUNT(*) FROM patch_in_list WHERE id = :ID");
		remainingEntries.bind(":ID", kLegacyImportId);
		REQUIRE(remainingEntries.executeStep());
		CHECK(remainingEntries.getColumn(0).getInt() == 0);
	}

	auto importsAfterDelete = db.getImportsList(dummySynth.get());
	CHECK(importsAfterDelete.empty());
}
