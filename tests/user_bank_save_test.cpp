#include "doctest/doctest.h"

#include "PatchDatabase.h"
#include "PatchListType.h"
#include "SynthBank.h"
#include "Patch.h"
#include "HasBanksCapability.h"
#include "The-Orm/UserBankFactory.h"

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>

#include <filesystem>
#include <map>
#include <random>
#include <string>

namespace {

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
	return ScopedTempFile(base / ("user_bank_save_" + makeRandomSuffix() + ".db3"));
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
	DummySynth(std::string name, int bankSize = 2) : name_(std::move(name)), bankSize_(bankSize) {}

	std::shared_ptr<midikraft::DataFile> patchFromPatchData(const Synth::PatchData& data, MidiProgramNumber) const override {
		auto patch = std::make_shared<DummyPatch>();
		patch->setData(data);
		return patch;
	}

	bool isOwnSysex(juce::MidiMessage const&) const override { return true; }

	std::string getName() const override { return name_; }

	int numberOfBanks() const override { return 1; }
	int numberOfPatches() const override { return bankSize_; }
	std::string friendlyBankName(MidiBankNumber) const override { return "Dummy Bank"; }
	std::vector<juce::MidiMessage> bankSelectMessages(MidiBankNumber) const override { return {}; }

private:
	std::string name_;
	int bankSize_;
};

midikraft::PatchHolder makePatchHolder(std::shared_ptr<DummySynth> synth, std::string const& name, std::vector<uint8> data) {
	auto patch = std::make_shared<DummyPatch>();
	patch->setData(data);
	auto sourceInfo = std::make_shared<midikraft::FromFileSource>(name + ".syx", "/tmp/" + name + ".syx", MidiProgramNumber::invalidProgram());
	midikraft::PatchHolder holder(synth, sourceInfo, patch);
	holder.setName(name);
	return holder;
}

std::vector<midikraft::PatchHolder> fillToCapacity(std::vector<midikraft::PatchHolder> patches, int capacity) {
	if (capacity <= 0 || patches.empty()) {
		return patches;
	}
	while (static_cast<int>(patches.size()) < capacity) {
		patches.push_back(patches.back());
	}
	return patches;
}

} // namespace

TEST_CASE("user bank edits are persisted when saved") {
	auto tmp = makeTempDatabasePath();
	midikraft::PatchDatabase db(tmp.path().string(), midikraft::PatchDatabase::OpenMode::READ_WRITE);

	auto synth = std::make_shared<DummySynth>("DummySynth", 2);
	auto patchA = makePatchHolder(synth, "Patch A", { 0x01, 0x02 });
	auto patchB = makePatchHolder(synth, "Patch B", { 0x03, 0x04 });
	auto patchC = makePatchHolder(synth, "Patch C", { 0x05, 0x06 });

	db.putPatch(patchA);
	db.putPatch(patchB);
	db.putPatch(patchC);

	auto bankNo = MidiBankNumber::fromZeroBase(0, synth->numberOfPatches());
	auto bankId = std::string("user-bank-") + makeRandomSuffix();
	auto bank = std::make_shared<midikraft::UserBank>(bankId, "Test User Bank", synth, bankNo);
	bank->setPatches(fillToCapacity({ patchA, patchB }, bankNo.bankSize()));
	db.putPatchList(bank);

	bank->changePatchAtPosition(MidiProgramNumber::fromZeroBaseWithBank(bankNo, 0), patchC);
	db.putPatchList(bank);

	std::map<std::string, std::weak_ptr<midikraft::Synth>> synthMap;
	synthMap[synth->getName()] = synth;

	auto reloaded = db.getPatchList({ bankId, "Test User Bank" }, synthMap);
	REQUIRE(reloaded);
	auto reloadedBank = std::dynamic_pointer_cast<midikraft::UserBank>(reloaded);
	REQUIRE(reloadedBank);
	REQUIRE(reloadedBank->patches().size() >= 2);
	CHECK(reloadedBank->patches()[0].md5() == patchC.md5());
	CHECK(reloadedBank->patches()[0].name() == patchC.name());
	CHECK(reloadedBank->id() == bankId);

	SQLite::Database verify(tmp.path().string(), SQLite::OPEN_READONLY);
	SQLite::Statement listQuery(verify, "SELECT list_type FROM lists WHERE id = :ID");
	listQuery.bind(":ID", bankId.c_str());
	REQUIRE(listQuery.executeStep());
	CHECK(listQuery.getColumn(0).getInt() == midikraft::PatchListType::USER_BANK);
}

TEST_CASE("user bank list-drop edits are persisted when saved") {
	auto tmp = makeTempDatabasePath();
	midikraft::PatchDatabase db(tmp.path().string(), midikraft::PatchDatabase::OpenMode::READ_WRITE);

	auto synth = std::make_shared<DummySynth>("DummySynth", 3);
	auto patchA = makePatchHolder(synth, "Patch A", { 0x01, 0x02 });
	auto patchB = makePatchHolder(synth, "Patch B", { 0x03, 0x04 });
	auto patchC = makePatchHolder(synth, "Patch C", { 0x05, 0x06 });
	auto patchD = makePatchHolder(synth, "Patch D", { 0x07, 0x08 });

	db.putPatch(patchA);
	db.putPatch(patchB);
	db.putPatch(patchC);
	db.putPatch(patchD);

	auto bankNo = MidiBankNumber::fromZeroBase(0, synth->numberOfPatches());
	auto bankId = std::string("user-bank-") + makeRandomSuffix();
	auto bank = std::make_shared<midikraft::UserBank>(bankId, "List Drop Bank", synth, bankNo);
	bank->setPatches(fillToCapacity({ patchA, patchB, patchC }, bankNo.bankSize()));
	db.putPatchList(bank);

	auto list = std::make_shared<midikraft::PatchList>("list-to-drop");
	list->setPatches({ patchD, patchA });
	bank->copyListToPosition(MidiProgramNumber::fromZeroBaseWithBank(bankNo, 1), *list);
	db.putPatchList(bank);

	std::map<std::string, std::weak_ptr<midikraft::Synth>> synthMap;
	synthMap[synth->getName()] = synth;

	auto reloaded = db.getPatchList({ bankId, "List Drop Bank" }, synthMap);
	REQUIRE(reloaded);
	auto reloadedBank = std::dynamic_pointer_cast<midikraft::UserBank>(reloaded);
	REQUIRE(reloadedBank);
	REQUIRE(reloadedBank->patches().size() >= 3);
	CHECK(reloadedBank->patches()[0].md5() == patchA.md5());
	CHECK(reloadedBank->patches()[1].md5() == patchD.md5());
	CHECK(reloadedBank->patches()[2].md5() == patchA.md5());

	SQLite::Database verify(tmp.path().string(), SQLite::OPEN_READONLY);
	SQLite::Statement listQuery(verify, "SELECT list_type FROM lists WHERE id = :ID");
	listQuery.bind(":ID", bankId.c_str());
	REQUIRE(listQuery.executeStep());
	CHECK(listQuery.getColumn(0).getInt() == midikraft::PatchListType::USER_BANK);
}

TEST_CASE("user bank rename persists and stays a user bank") {
	auto tmp = makeTempDatabasePath();
	midikraft::PatchDatabase db(tmp.path().string(), midikraft::PatchDatabase::OpenMode::READ_WRITE);

	auto synth = std::make_shared<DummySynth>("DummySynth");
	auto patchA = makePatchHolder(synth, "Patch A", { 0x01, 0x02 });
	db.putPatch(patchA);

	auto bankNo = MidiBankNumber::fromZeroBase(0, synth->numberOfPatches());
	auto bankId = std::string("user-bank-") + makeRandomSuffix();
	auto bank = std::make_shared<midikraft::UserBank>(bankId, "Original Name", synth, bankNo);
	bank->setPatches(fillToCapacity({ patchA }, bankNo.bankSize()));
	db.putPatchList(bank);

	bank->setName("Renamed Bank");
	db.putPatchList(bank);

	std::map<std::string, std::weak_ptr<midikraft::Synth>> synthMap;
	synthMap[synth->getName()] = synth;
	auto reloaded = db.getPatchList({ bankId, "Renamed Bank" }, synthMap);
	REQUIRE(reloaded);
	auto reloadedBank = std::dynamic_pointer_cast<midikraft::UserBank>(reloaded);
	REQUIRE(reloadedBank);
	CHECK(reloadedBank->name() == "Renamed Bank");

	SQLite::Database verify(tmp.path().string(), SQLite::OPEN_READONLY);
	SQLite::Statement listQuery(verify, "SELECT list_type FROM lists WHERE id = :ID");
	listQuery.bind(":ID", bankId.c_str());
	REQUIRE(listQuery.executeStep());
	CHECK(listQuery.getColumn(0).getInt() == midikraft::PatchListType::USER_BANK);
}

TEST_CASE("user bank factory creates user bank list entries") {
	auto tmp = makeTempDatabasePath();
	midikraft::PatchDatabase db(tmp.path().string(), midikraft::PatchDatabase::OpenMode::READ_WRITE);

	auto synth = std::make_shared<DummySynth>("DummySynth");
	auto patchA = makePatchHolder(synth, "Patch A", { 0x01, 0x02 });
	db.putPatch(patchA);

	auto bank = knobkraft::createUserBank(synth, 0, "Factory Bank");
	REQUIRE(bank);
	bank->setPatches(fillToCapacity({ patchA }, bank->bankNumber().bankSize()));
	db.putPatchList(bank);

	std::map<std::string, std::weak_ptr<midikraft::Synth>> synthMap;
	synthMap[synth->getName()] = synth;
	auto reloaded = db.getPatchList({ bank->id(), bank->name() }, synthMap);
	REQUIRE(reloaded);
	auto reloadedBank = std::dynamic_pointer_cast<midikraft::UserBank>(reloaded);
	REQUIRE(reloadedBank);
	CHECK(reloadedBank->id() == bank->id());

	SQLite::Database verify(tmp.path().string(), SQLite::OPEN_READONLY);
	SQLite::Statement listQuery(verify, "SELECT list_type FROM lists WHERE id = :ID");
	listQuery.bind(":ID", bank->id().c_str());
	REQUIRE(listQuery.executeStep());
	CHECK(listQuery.getColumn(0).getInt() == midikraft::PatchListType::USER_BANK);
}
