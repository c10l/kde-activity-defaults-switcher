# 15: Stop overclaiming the mime-key test

**What to build:** `mimeKeysMatchSystemSettingsComponentChooser` (`tests/tst_writer.cpp`) asserts the hardcoded mime lists equal the function's output — it pins the values against drift but does **not** verify them against the plasma-workspace component chooser, while its comment claims they are "exactly" what the component chooser defines. Either verify the values against the real plasma-workspace source (and keep the claim, citing it), or soften the comment to say the test pins the values against drift.

**Blocked by:** 03 (Writer).

**Status:** done

- [x] The test comment accurately reflects what the test verifies (pin-against-drift vs verified-against-upstream).
- [x] If claiming parity with plasma-workspace, the source is cited/linked and the values are actually checked against it.

**Suggested skills:** `work-ticket`; use `research` if verifying against the upstream plasma-workspace source. `review-ticket` (reviewer).

**References:**
- `tests/tst_writer.cpp` — `mimeKeysMatchSystemSettingsComponentChooser`.
- `.scratch/activity-defaults-switcher/spec.md` — the *Components* decision.
- `CONTEXT.md` — vocabulary.
