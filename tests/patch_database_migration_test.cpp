#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "../third_party/cereal/unittests/doctest.h"

#include "PatchDatabase.h"

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>

#include <filesystem>
#include <random>
#include <string>

namespace {

constexpr int kLegacySchemaVersion = 16;
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

std::string makeRandomSuffix() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 0xFFFFFF);
    return std::to_string(dist(gen));
}

std::filesystem::path makeTempDatabasePath() {
    auto base = std::filesystem::temp_directory_path();
    return base / ("patch_database_migration_" + makeRandomSuffix() + ".db3");
}

void createLegacyImportDatabase(const std::filesystem::path& dbPath) {
    std::error_code ec;
    std::filesystem::remove(dbPath, ec);
    SQLite::Database db(dbPath.string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

    db.exec("CREATE TABLE schema_version (number INTEGER)");
    SQLite::Statement insertVersion(db, "INSERT INTO schema_version (number) VALUES (?)");
    insertVersion.bind(1, kLegacySchemaVersion);
    insertVersion.exec();

    db.exec("CREATE TABLE patches (synth TEXT NOT NULL, md5 TEXT NOT NULL, name TEXT, type INTEGER, data BLOB, favorite INTEGER, regular INTEGER, hidden INTEGER, sourceID TEXT, sourceName TEXT, sourceInfo TEXT, midiBankNo INTEGER, midiProgramNo INTEGER, categories INTEGER, categoryUserDecision INTEGER, comment TEXT, author TEXT, info TEXT, PRIMARY KEY (synth, md5))");
    db.exec("CREATE TABLE imports (synth TEXT, name TEXT, id TEXT, date TEXT)");
    db.exec("CREATE TABLE lists(id TEXT PRIMARY KEY, name TEXT NOT NULL, synth TEXT, midi_bank_number INTEGER, last_synced INTEGER)");
    db.exec("CREATE TABLE patch_in_list(id TEXT NOT NULL, synth TEXT NOT NULL, md5 TEXT NOT NULL, order_num INTEGER NOT NULL)");
    db.exec("CREATE TABLE categories (bitIndex INTEGER UNIQUE, name TEXT, color TEXT, active INTEGER, sort_order INTEGER)");

    SQLite::Statement insertPatch(db,
        "INSERT INTO patches (synth, md5, name, type, data, favorite, regular, hidden, sourceID, sourceName, sourceInfo, midiBankNo, midiProgramNo, categories, categoryUserDecision, comment, author, info) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    insertPatch.bind(1, kLegacySynth);
    insertPatch.bind(2, kLegacyMd5);
    insertPatch.bind(3, "Bass 01");
    insertPatch.bind(4, 0);
    insertPatch.bind(5); // NULL blob
    insertPatch.bind(6, 0);  // favorite
    insertPatch.bind(7, 0);  // regular
    insertPatch.bind(8, 0);  // hidden
    insertPatch.bind(9, kLegacyImportId);
    insertPatch.bind(10, kLegacyImportName);
    insertPatch.bind(11, R"({"bulksource":true,"timestamp":"2024-01-01T12:00:00Z"})");
    insertPatch.bind(12, 0); // midiBankNo
    insertPatch.bind(13, 0); // midiProgramNo
    insertPatch.bind(14, 0); // categories
    insertPatch.bind(15, 0); // categoryUserDecision
    insertPatch.bind(16, "");
    insertPatch.bind(17, "");
    insertPatch.bind(18, "");
    insertPatch.exec();

    SQLite::Statement insertImport(db, "INSERT INTO imports (synth, name, id, date) VALUES (?, ?, ?, ?)");
    insertImport.bind(1, kLegacySynth);
    insertImport.bind(2, kLegacyImportName);
    insertImport.bind(3, kLegacyImportId);
    insertImport.bind(4, "2024-01-01 12:00:00");
    insertImport.exec();
}

} // namespace

TEST_CASE("legacy imports migrate into list records with zero-based ordering") {
    auto dbPath = makeTempDatabasePath();
    createLegacyImportDatabase(dbPath);

    {
        midikraft::PatchDatabase db(dbPath.string(), midikraft::PatchDatabase::OpenMode::READ_WRITE);
    }

    SQLite::Database verify(dbPath.string(), SQLite::OPEN_READONLY);

    SQLite::Statement importListQuery(verify, "SELECT list_type, synth FROM lists WHERE id = :ID");
    importListQuery.bind(":ID", kLegacyImportId);
    REQUIRE(importListQuery.executeStep());
    CHECK(importListQuery.getColumn("list_type").getInt() == ListType::IMPORT_LIST);
    CHECK(std::string(importListQuery.getColumn("synth").getText()) == kLegacySynth);

    SQLite::Statement pilQuery(verify, "SELECT order_num FROM patch_in_list WHERE id = :ID AND md5 = :MD5");
    pilQuery.bind(":ID", kLegacyImportId);
    pilQuery.bind(":MD5", kLegacyMd5);
    REQUIRE(pilQuery.executeStep());
    CHECK(pilQuery.getColumn(0).getInt() == 0);
}
