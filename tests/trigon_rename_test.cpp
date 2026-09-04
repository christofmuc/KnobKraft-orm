#include "doctest/doctest.h"

#include "GenericAdaptation.h"
#include "PatchDatabase.h"
#include "Sysex.h"

#include <pybind11/embed.h>

#include <filesystem>
#include <memory>
#include <vector>

TEST_CASE("Trigon-6 reimport preserves custom names over Basic Program") {
	pybind11::scoped_interpreter python;
	auto adaptationsPath = std::filesystem::path(KNOBKRAFT_TEST_SOURCE_DIR) / "adaptations";
	pybind11::module_::import("sys").attr("path").attr("insert")(0, adaptationsPath.string());
	auto synth = std::make_shared<knobkraft::GenericAdaptation>("Sequential_Trigon6");
	auto fixture = Sysex::loadSysex((adaptationsPath / "testData" / "Sequential_Trigon6" / "T6_Programs_v1.0.syx").string());
	REQUIRE(fixture.size() == 500);

	// Load separate copies, as PatchHolder::setName also modifies the patch data.
	auto makePatch = [&]() {
		auto patches = synth->loadSysex({ fixture[84] });
		REQUIRE(patches.size() == 1);
		auto source = std::make_shared<midikraft::FromFileSource>(
			"T6_Programs_v1.0.syx", (adaptationsPath / "testData" / "Sequential_Trigon6" / "T6_Programs_v1.0.syx").string(),
			MidiProgramNumber::fromZeroBase(84));
		return midikraft::PatchHolder(synth, source, patches.front());
	};
	auto renamed = makePatch();
	auto originalFingerprint = renamed.md5();
	renamed.setName("084 CCJ Brass");
	REQUIRE(renamed.name() == "084 CCJ Brass");
	REQUIRE(renamed.md5() == originalFingerprint);

	midikraft::PatchDatabase db(":memory:", midikraft::PatchDatabase::OpenMode::READ_WRITE);
	db.putPatch(renamed);
	auto incoming = makePatch();
	incoming.setName("Basic Program");
	REQUIRE(incoming.name() == "Basic Program");
	REQUIRE(incoming.md5() == originalFingerprint);

	auto reimport = [&](midikraft::PatchHolder patch) {
		std::vector<midikraft::PatchHolder> patches{ patch }, added;
		db.mergePatchesIntoDatabase(patches, added, nullptr,
			midikraft::PatchDatabase::UPDATE_NAME | midikraft::PatchDatabase::UPDATE_CATEGORIES | midikraft::PatchDatabase::UPDATE_FAVORITE);
		CHECK(added.empty());
		std::vector<midikraft::PatchHolder> stored;
		REQUIRE(db.getSinglePatch(synth, originalFingerprint, stored));
		REQUIRE(stored.size() == 1);
		return stored.front();
	};
	CHECK(reimport(incoming).name() == "084 CCJ Brass");

	// Ordinary names from the synth must still be accepted on reimport.
	incoming.setName("New Brass");
	CHECK(reimport(incoming).name() == "New Brass");
}
