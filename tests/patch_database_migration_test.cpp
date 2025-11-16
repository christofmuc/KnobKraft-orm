#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest/doctest.h"

#include "PatchDatabase.h"
#include "PatchListType.h"
#include "ImportList.h"
#include "Patch.h"
#include "HasBanksCapability.h"

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>

#include <filesystem>
#include <map>
#include <algorithm>
#include <iterator>
#include <random>
#include <string>

namespace {

constexpr int kLegacySchemaVersion = 13;
constexpr auto kLegacySynth = "TestSynth";
constexpr auto kLegacyMd5 = "md5-aaa";
constexpr auto kSecondMd5 = "md5-bbb";
constexpr auto kSecondSynth = "TestSynthB";
constexpr auto kSecondSynthMd5 = "md5-ccc";
constexpr auto kLegacyImportId = "import-legacy-001";
constexpr auto kLegacyImportName = "Legacy Bulk Import";
constexpr auto kSecondPatchName = "Bass 02";
const std::string kPrefixedImportId = std::string("import:") + kLegacySynth + ":" + kLegacyImportId;
const std::string kSecondPrefixedImportId = std::string("import:") + kSecondSynth + ":" + kLegacyImportId;

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

void insertSecondLegacyPatch(const std::filesystem::path& dbPath) {
	SQLite::Database db(dbPath.string(), SQLite::OPEN_READWRITE);

	SQLite::Statement updateProgram(db, "UPDATE patches SET midiProgramNo = ? WHERE md5 = ?");
	updateProgram.bind(1, 64);
	updateProgram.bind(2, kLegacyMd5);
	updateProgram.exec();

	SQLite::Statement insertPatch(db,
		"INSERT INTO patches (synth, md5, name, type, data, favorite, hidden, sourceID, sourceName, sourceInfo, midiBankNo, midiProgramNo, categories, categoryUserDecision, comment) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
	constexpr unsigned char kSecondPatchBytes[] = { 0x05, 0x06, 0x07, 0x08 };
	insertPatch.bind(1, kLegacySynth);
	insertPatch.bind(2, kSecondMd5);
	insertPatch.bind(3, kSecondPatchName);
	insertPatch.bind(4, 0);
	insertPatch.bind(5, kSecondPatchBytes, static_cast<int>(sizeof(kSecondPatchBytes)));
	insertPatch.bind(6, 0);  // favorite
	insertPatch.bind(7, 0);  // hidden
	insertPatch.bind(8, kLegacyImportId);
	insertPatch.bind(9, kLegacyImportName);
	insertPatch.bind(10, R"({"bulksource":true,"timestamp":"2024-01-01T12:05:00Z"})");
	insertPatch.bind(11, 0); // midiBankNo
	insertPatch.bind(12, 1); // midiProgramNo
	insertPatch.bind(13, 0); // categories
	insertPatch.bind(14, 0); // categoryUserDecision
	insertPatch.bind(15, ""); // comment
	insertPatch.exec();
}

void insertSecondSynthLegacyImport(const std::filesystem::path& dbPath) {
	SQLite::Database db(dbPath.string(), SQLite::OPEN_READWRITE);

	SQLite::Statement insertPatch(db,
		"INSERT INTO patches (synth, md5, name, type, data, favorite, hidden, sourceID, sourceName, sourceInfo, midiBankNo, midiProgramNo, categories, categoryUserDecision, comment) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
	insertPatch.bind(1, kSecondSynth);
	insertPatch.bind(2, kSecondSynthMd5);
	insertPatch.bind(3, "Pad 01");
	insertPatch.bind(4, 0);
	constexpr unsigned char kPatchBytes[] = { 0x05, 0x06, 0x07, 0x08 };
	insertPatch.bind(5, kPatchBytes, static_cast<int>(std::size(kPatchBytes)));
	insertPatch.bind(6, 0);  // favorite
	insertPatch.bind(7, 0);  // hidden
	insertPatch.bind(8, kLegacyImportId);
	insertPatch.bind(9, kLegacyImportName);
	insertPatch.bind(10, R"({"bulksource":true,"timestamp":"2024-02-02T12:00:00Z"})");
	insertPatch.bind(11, 0); // midiBankNo
	insertPatch.bind(12, 0); // midiProgramNo
	insertPatch.bind(13, 0); // categories
	insertPatch.bind(14, 0); // categoryUserDecision
	insertPatch.bind(15, ""); // comment
	insertPatch.exec();

	SQLite::Statement insertImport(db, "INSERT INTO imports (synth, name, id, date) VALUES (?, ?, ?, ?)");
	insertImport.bind(1, kSecondSynth);
	insertImport.bind(2, kLegacyImportName);
	insertImport.bind(3, kLegacyImportId);
	insertImport.bind(4, "2024-02-02 12:00:00");
	insertImport.exec();
}

bool tableHasColumn(SQLite::Database& db, std::string const& table, std::string const& column) {
	auto pragma = "PRAGMA table_info(" + table + ")";
	SQLite::Statement stmt(db, pragma.c_str());
	while (stmt.executeStep()) {
		if (column == stmt.getColumn("name").getText()) {
			return true;
		}
	}
	return false;
}

bool hasIndex(SQLite::Database& db, std::string const& indexName) {
	SQLite::Statement stmt(db, "SELECT 1 FROM sqlite_master WHERE type = 'index' AND name = :NAME");
	stmt.bind(":NAME", indexName.c_str());
	return stmt.executeStep();
}

std::vector<std::string> orderedPatchNames(SQLite::Database& db, std::string const& listId) {
	SQLite::Statement stmt(db,
		"SELECT patches.name "
		"  FROM patch_in_list "
		"  JOIN patches ON patches.md5 = patch_in_list.md5 AND patches.synth = patch_in_list.synth "
		" WHERE patch_in_list.id = :ID "
		" ORDER BY patch_in_list.order_num");
	stmt.bind(":ID", listId.c_str());
	std::vector<std::string> names;
	while (stmt.executeStep()) {
		names.emplace_back(stmt.getColumn(0).getText());
	}
	return names;
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
		importListQuery.bind(":ID", kPrefixedImportId.c_str());
		REQUIRE(importListQuery.executeStep());
		CHECK(importListQuery.getColumn("list_type").getInt() == midikraft::PatchListType::IMPORT_LIST);
		CHECK(std::string(importListQuery.getColumn("synth").getText()) == kLegacySynth);
		CHECK(importListQuery.getColumn("last_synced").getInt64() > 0);

		SQLite::Statement pilQuery(verify, "SELECT order_num FROM patch_in_list WHERE id = :ID AND md5 = :MD5");
		pilQuery.bind(":ID", kPrefixedImportId.c_str());
		pilQuery.bind(":MD5", kLegacyMd5);
		REQUIRE(pilQuery.executeStep());
		CHECK(pilQuery.getColumn(0).getInt() == 0);
	}

	midikraft::PatchDatabase db(tmp.path().string(), midikraft::PatchDatabase::OpenMode::READ_WRITE);
	auto dummySynth = std::make_shared<DummySynth>(kLegacySynth);

	{
		SQLite::Database verify(tmp.path().string(), SQLite::OPEN_READONLY);
		SQLite::Statement listQuery(verify, "SELECT name FROM lists WHERE id = :ID");
		listQuery.bind(":ID", kPrefixedImportId.c_str());
		REQUIRE(listQuery.executeStep());
		CHECK(std::string(listQuery.getColumn(0).getText()) == kLegacyImportName);
		SQLite::Statement countQuery(verify, "SELECT COUNT(*) FROM patch_in_list WHERE id = :ID");
		countQuery.bind(":ID", kPrefixedImportId.c_str());
		REQUIRE(countQuery.executeStep());
		CHECK(countQuery.getColumn(0).getInt() == 1);
	}

	std::map<std::string, std::weak_ptr<midikraft::Synth>> synthMap;
	synthMap[kLegacySynth] = dummySynth;
	auto list = db.getPatchList({ kPrefixedImportId, kLegacyImportName }, synthMap);
	REQUIRE(list);
	auto importList = std::dynamic_pointer_cast<midikraft::ImportList>(list);
	REQUIRE(importList);
	auto patchesInList = importList->patches();
	REQUIRE(patchesInList.size() == 1);
	CHECK(patchesInList.front().synth()->getName() == kLegacySynth);
	CHECK(patchesInList.front().name() == "Bass 01");
	CHECK(patchesInList.front().synth()->getName() == kLegacySynth);

	db.removePatchFromList(kPrefixedImportId, kLegacySynth, kLegacyMd5, 0);

	{
		SQLite::Database verify(tmp.path().string(), SQLite::OPEN_READONLY);
		SQLite::Statement remainingEntries(verify, "SELECT COUNT(*) FROM patch_in_list WHERE id = :ID");
		remainingEntries.bind(":ID", kPrefixedImportId.c_str());
		REQUIRE(remainingEntries.executeStep());
		CHECK(remainingEntries.getColumn(0).getInt() == 0);
	}

	{
		SQLite::Database verify(tmp.path().string(), SQLite::OPEN_READONLY);
		SQLite::Statement countQuery(verify, "SELECT COUNT(*) FROM patch_in_list WHERE id = :ID");
		countQuery.bind(":ID", kPrefixedImportId.c_str());
		REQUIRE(countQuery.executeStep());
		CHECK(countQuery.getColumn(0).getInt() == 0);
	}
}

TEST_CASE("import ordering uses list order when sourceIDs are empty") {
	auto tmp = makeTempDatabasePath();
	createLegacyImportDatabase(tmp.path());

	insertSecondLegacyPatch(tmp.path());

	{
		midikraft::PatchDatabase migrator(tmp.path().string(), midikraft::PatchDatabase::OpenMode::READ_WRITE);
	}

	{
		SQLite::Database db(tmp.path().string(), SQLite::OPEN_READWRITE);
		SQLite::Statement normalize(db, "UPDATE patch_in_list SET order_num = CASE WHEN md5 = :FIRST THEN 0 WHEN md5 = :SECOND THEN 1 ELSE order_num END WHERE id = :ID");
		normalize.bind(":FIRST", kLegacyMd5);
		normalize.bind(":SECOND", kSecondMd5);
		normalize.bind(":ID", kPrefixedImportId.c_str());
		normalize.exec();
	}

	midikraft::PatchDatabase database(tmp.path().string(), midikraft::PatchDatabase::OpenMode::READ_WRITE);
	auto synth = std::make_shared<DummySynth>(kLegacySynth);

	std::map<std::string, std::weak_ptr<midikraft::Synth>> synthMap;
	synthMap[kLegacySynth] = synth;
	midikraft::PatchFilter filter(synthMap);
	filter.orderBy = midikraft::PatchOrdering::Order_by_Import_id;

	auto patches = database.getPatches(filter, 0, -1);
	REQUIRE(patches.size() == 2);

	std::vector<std::string> actualNames;
	std::transform(patches.begin(), patches.end(), std::back_inserter(actualNames),
		[](auto const& patch) { return patch.name(); });

	SQLite::Database verify(tmp.path().string(), SQLite::OPEN_READONLY);
	auto expectedNames = orderedPatchNames(verify, kPrefixedImportId);
	CHECK(actualNames == expectedNames);
}

TEST_CASE("schema migration drops sourceID column and index") {
	auto tmp = makeTempDatabasePath();
	createLegacyImportDatabase(tmp.path());
	insertSecondLegacyPatch(tmp.path());

	midikraft::PatchDatabase database(tmp.path().string(), midikraft::PatchDatabase::OpenMode::READ_WRITE);
	auto synth = std::make_shared<DummySynth>(kLegacySynth);

	std::map<std::string, std::weak_ptr<midikraft::Synth>> synthMap;
	synthMap[kLegacySynth] = synth;
	midikraft::PatchFilter filter(synthMap);
	filter.orderBy = midikraft::PatchOrdering::Order_by_Import_id;

	auto patches = database.getPatches(filter, 0, -1);
	REQUIRE(patches.size() == 2);

	std::vector<std::string> actualNames;
	std::transform(patches.begin(), patches.end(), std::back_inserter(actualNames),
		[](auto const& patch) { return patch.name(); });

	SQLite::Database verify(tmp.path().string(), SQLite::OPEN_READONLY);
	auto expectedNames = orderedPatchNames(verify, kPrefixedImportId);
	CHECK(actualNames == expectedNames);
	CHECK_FALSE(tableHasColumn(verify, "patches", "sourceID"));
	CHECK_FALSE(hasIndex(verify, "patch_sourceid_idx"));
}

TEST_CASE("duplicate import ids per synth migrate without unique constraint issues") {
	auto tmp = makeTempDatabasePath();
	createLegacyImportDatabase(tmp.path());
	insertSecondSynthLegacyImport(tmp.path());

	// Migration should complete even though imports share the same id across different synths
	REQUIRE_NOTHROW(midikraft::PatchDatabase(tmp.path().string(), midikraft::PatchDatabase::OpenMode::READ_WRITE));

	SQLite::Database verify(tmp.path().string(), SQLite::OPEN_READONLY);

	SQLite::Statement listCount(verify, "SELECT COUNT(*) FROM lists WHERE id IN (:FIRST, :SECOND)");
	listCount.bind(":FIRST", kPrefixedImportId.c_str());
	listCount.bind(":SECOND", kSecondPrefixedImportId.c_str());
	REQUIRE(listCount.executeStep());
	CHECK(listCount.getColumn(0).getInt() == 2);

	SQLite::Statement pilA(verify, "SELECT synth, md5 FROM patch_in_list WHERE id = :ID");
	pilA.bind(":ID", kPrefixedImportId.c_str());
	REQUIRE(pilA.executeStep());
	CHECK(std::string(pilA.getColumn(0).getText()) == kLegacySynth);
	CHECK(std::string(pilA.getColumn(1).getText()) == kLegacyMd5);

	SQLite::Statement pilB(verify, "SELECT synth, md5 FROM patch_in_list WHERE id = :ID");
	pilB.bind(":ID", kSecondPrefixedImportId.c_str());
	REQUIRE(pilB.executeStep());
	CHECK(std::string(pilB.getColumn(0).getText()) == kSecondSynth);
	CHECK(std::string(pilB.getColumn(1).getText()) == kSecondSynthMd5);
}
