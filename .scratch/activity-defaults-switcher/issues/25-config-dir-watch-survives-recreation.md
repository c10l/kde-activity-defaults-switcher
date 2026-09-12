# 25: Config-directory watch survives deletion and recreation

**What to build:** `ActivityDefaultsSwitcherModule` (`src/module.cpp`) watches the config directory once it exists (`upgradeConfigDirWatch`), so `mimeapps.list`/`kdeglobals`/the profile store created after startup are picked up. Ticket 10's review note recorded a boundary: if the config directory itself is deleted while the module runs, `QFileSystemWatcher` drops the watch, and recreation produces no event because the nearest ancestor is no longer watched — the watch is only re-established at the next daemon start. The note judged the impact practically nil (deleting `~/.config` under a live session already breaks the session, and login re-watches) and was accepted as deferred with no follow-up ticket.

**Verification first (grill the user):** Before writing code, verify the note against the current `src/module.cpp` (`ensureConfigWatched`, `upgradeConfigDirWatch`, the `directoryChanged` handling) with a failing shell test or a reasoned trace, then interview the user on whether this edge is worth fixing and, if so, how (watch an ancestor that survives the deletion, re-check the directory after a disappearance, or re-arm on the next event). Implement only after that.

**Blocked by:** 10 (watch config dir when created), 08 (kded shell tests).

**Status:** ready-for-agent

- [ ] The note is verified against the current module: a test or trace shows the watch is dropped when the config directory is deleted and is not re-established on recreation within the same run.
- [ ] The user is grilled on whether to fix it and the approach, before any code changes.
- [ ] If acted on: deleting and recreating the config directory while the module runs re-establishes the watch, and subsequent edits to `mimeapps.list`/`kdeglobals`/the store trigger capture; pinned by a shell test with injected fakes (per ticket 08).
- [ ] If declined: the decision and rationale are recorded in this ticket.

**Suggested skills:** `work-ticket` (implementer — verify the note and grill the user); `review-ticket` (reviewer).

**References:**
- `.scratch/activity-defaults-switcher/issues/10-watch-config-dir-when-created.md` — "Notes from review of 10".
- `src/module.cpp` — `ensureConfigWatched`, `upgradeConfigDirWatch`, `directoryChanged`.
- `tests/tst_module.cpp` — the shell tests (`configDirCreatedAfterStartupIsWatchedAndCaptureRuns`, `recreatedFileIsReWatchedAndCaptureRuns`).
- `.scratch/activity-defaults-switcher/issues/08-kded-shell-tests.md` — the injected-fakes seam.
