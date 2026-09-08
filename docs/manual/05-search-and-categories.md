# Search and categories

[Manual contents](../index.md)

## Start a fresh search

1. Select a synth's library to search that synth, or **All patches** to search across active synths.
2. Click **Clear filters**.
3. Type a distinctive fragment in the text search box.
4. Choose category buttons to narrow the results, if useful.
5. Choose a sort order and browse the resulting pages.

**Expected result:** the grid contains patches matching the selected library scope and search conditions. Ordinary text search checks patch name, comment, author, and info. It is not an audio similarity search. Text beginning with `!` enters a separate advanced query path, outside this beginner workflow.

**Clear filters** resets the search controls while keeping the sort choice. It does not own the navigation tree's list filter. To search outside a selected import, list, or bank, also select the synth library or **All patches**. The application keeps separate search state for individual synths and multi-synth browsing, so changing synth can restore a different set of filters.

Available sort choices are **Sort by import**, **Sort by name**, **Sort by program #**, and **Sort by bank #**. User lists and banks use their stored order in the current implementation. Patch display choices include **Name and #**, **Name**, **Program #**, **Layers and #**, and **Name and author**; layer information depends on the patch type.

## Find favorites or recover hidden sounds

The status filters are **Faves**, **Hidden**, **Regular**, and **Undecided**. Undecided means no favorite, hidden, or regular flag. With no status filter active, the query shows non-hidden patches.

To find favorites, start from a fresh search and enable **Faves**. To find a hidden patch, start fresh, enable **Hidden**, and search for its name. If a patch has both favorite and hidden flags, enable both **Faves** and **Hidden**: an inactive status filter can exclude patches with that flag. This overlap matters when diagnosing a missing favorite.

Once you find the patch, select it and change **Hide** in **Current Patch**. Refresh or change the filter to see the resulting membership.

**Duplicate Names** finds database entries with repeated names. It does not prove their sound data is identical. **Untagged** finds patches with no categories and takes precedence over category selection in the query.

## Filter by categories

Choose one or more category buttons in the search area. With **All must match** off, a patch can match any selected category. Turn it on to require all selected categories.

Category buttons in **Current Patch** edit a patch's classification; category buttons in search filter what you see. Keep these two tasks distinct.

## Assign a category and add useful notes

1. Select the patch and open **Current Patch**.
2. Toggle its category buttons to describe the sound.
3. Add an **Author**, **Info**, or **Comment**, if useful.
4. Search for that category or a word from your comment to find the patch again.

**Expected result:** those metadata changes are written to the database. They do not automatically store the patch to a hardware program.

## Create a category

1. Choose **Categories → Edit categories**.
2. Click **Add new category**.
3. Give the new row a useful name, enable **Active**, and adjust **Order** and **Color** if needed.
4. Click **Save**.
5. Assign the category to a patch and test a search for it.

**Expected result:** the category definition is saved in the open database and active category buttons refresh.

## Reapply automatic classification

Automatic categorization uses naming rules and imported-category mappings. The menu exposes **Show category naming rules file** and **Edit category import mapping** to open or reveal those files. Editing their syntax is an advanced task; keep a copy of the original file before making changes.

After changing a rule or mapping:

1. Filter down to a small, known set of patches.
2. Choose **Categories → Rerun auto categorize...**.
3. Check the affected count in the confirmation and proceed only if it is the set you intend.
4. Inspect example patches to see whether the change had the expected effect.

**Expected result:** categorization is recalculated for the current filter. The implementation preserves recorded manual category decisions, including categories manually removed. It does not classify by listening to audio.
