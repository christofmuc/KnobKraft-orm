## Synths:

* **\#592** **Yamaha DX7** and **Korg M1** now recognize renamed copies of the same sound as duplicates. DX7 voices can be renamed, voice and bank imports accept any MIDI channel, and individual voice uploads use the configured channel. Thanks to @roomi-fields for the contribution and follow-up fixes! Channel handling is covered by automated fixture tests and still needs hardware confirmation. DX7 full-bank uploads currently use MIDI channel 1; the remaining callback limitation is tracked in [#596](https://github.com/christofmuc/KnobKraft-orm/issues/596).

  **Existing DX7 and M1 libraries need reindexing before reimporting any patches.** Back up your database and recordings, then restart KnobKraft after updating. Select **Yamaha DX7** and run **Edit → Reindex patches…**. Then select **Korg M1** and run the same command. Existing entries retain their old fingerprints until reindexing, so even identical dumps can create duplicate entries if reimported first. Reindexing includes hidden patches and creates a database backup. Equivalent sounds may merge and the retained name may differ. Existing DX7 names now follow the synth's ten-character limit.

  Recordings named by old fingerprints are not automatically renamed. Keep the originals and copy them under the new fingerprints if needed, avoiding overwrites when several patches merge. Thumbnails can be regenerated.
