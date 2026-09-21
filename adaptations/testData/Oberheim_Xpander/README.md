# Oberheim Xpander / Matrix-12 test fixtures

Sources:
- rubidium.se Xpander patches page
  - Factory1.zip
  - Factory2.zip
  - Sanchez.zip
  - Springer.zip
  - Johnson.zip
  - Urbanek.zip
  - XevenT.zip
- presetpatch.com Matrix_12.syx provided locally by the user

Notes:
- `rubidium_FACTORY1_XP.syx`, `rubidium_FACTORY2_M12.syx`, `rubidium_SPRINGER.syx`, `rubidium_JOHNSON1.syx`, and `rubidium_URBANEK.syx` are clean 100-message bank dumps that the current Xpander parser recognizes as 100 single-program dumps.
- `rubidium_SANCHEZ.syx`, `rubidium_XP_94.syx`, and `presetpatch_Matrix_12.syx` contain additional non-program messages alongside program dumps. These are useful robustness fixtures.
- `rubidium_single_09_weirdlfo.syx` contains one real single-program dump stored to slot 99 followed by the trailing 7-byte command used by the archive author.
- Current KnobKraft `loadSysex()` can double-parse clean bank streams when they are also valid single-program dumps. See `BUG_bank_dump_program_dump_double_parse.md` in the repo root.
