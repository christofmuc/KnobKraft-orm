/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Patch.h"
#include "StoredTagCapability.h"
#include "StoredPatchNameCapability.h"

namespace midikraft {

	class VirusPatch : public Patch, public StoredPatchNameCapability, public DefaultNameCapability, public StoredTagCapability {
	public:
		VirusPatch(Synth::PatchData const &data, MidiProgramNumber place);

		virtual std::string name() const override;
		virtual MidiProgramNumber patchNumber() const override;
		
		// StoredPatchNameCapability
		virtual void setName(std::string const &name) override;
		// DefaultNameCapability
		virtual bool isDefaultName(std::string const &patchName) const override;

		// StoredTagCapability
		bool setTags(std::set<Tag> const &tags) override;
		std::set<Tag> tags() const override;

		enum DataFileTypes { PATCH_VIRUS_B = 0 };
		enum Page { PageA = 0, PageB = 1 };
		static int index(Page page, int index);

	private:
		MidiProgramNumber place_;
	};

}
