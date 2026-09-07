# 20: Rename the mime-key test to reflect pin-against-drift

**What to build:** `mimeKeysMatchSystemSettingsComponentChooser` (`tests/tst_writer.cpp`) still names itself as matching the System Settings component chooser — the same overclaim ticket 15 removed from the comment. Rename it to something that states what it actually does (e.g. `mimeKeyListsPinAgainstDrift`), updating the slot declaration and definition.

**Blocked by:** 15 (Stop overclaiming the mime-key test).

**Status:** ready-for-agent

- [ ] The test slot is renamed to a name that does not claim a match against the System Settings component chooser.
- [ ] The suite still builds and passes (renaming a QTest slot must not break moc registration).

**Suggested skills:** `work-ticket` (implementer); `review-ticket` (reviewer).

**References:**
- `tests/tst_writer.cpp` — `mimeKeysMatchSystemSettingsComponentChooser`.
- `.scratch/activity-defaults-switcher/issues/15-mime-key-test-accuracy.md`.
