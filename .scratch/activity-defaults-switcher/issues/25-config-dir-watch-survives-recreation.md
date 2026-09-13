# 25: Config-directory watch survives deletion and recreation

**What to build:** `ActivityDefaultsSwitcherModule` (`src/module.cpp`) watches the config directory once it exists (`upgradeConfigDirWatch`), so `mimeapps.list`/`kdeglobals`/the profile store created after startup are picked up. Ticket 10's review note recorded a boundary: if the config directory itself is deleted while the module runs, `QFileSystemWatcher` drops the watch, and recreation produces no event because the nearest ancestor is no longer watched — the watch is only re-established at the next daemon start. The note judged the impact practically nil (deleting `~/.config` under a live session already breaks the session, and login re-watches) and was accepted as deferred with no follow-up ticket.

**Verification first (grill the user):** Before writing code, verify the note against the current `src/module.cpp` (`ensureConfigWatched`, `upgradeConfigDirWatch`, the `directoryChanged` handling) with a failing shell test or a reasoned trace, then interview the user on whether this edge is worth fixing and, if so, how (watch an ancestor that survives the deletion, re-check the directory after a disappearance, or re-arm on the next event). Implement only after that.

**Blocked by:** 10 (watch config dir when created), 08 (kded shell tests).

**Status:** done

- [x] The note is verified against the current module — and **disproved**: `configDirDeletedAndRecreatedIsRewatchedAndCaptureRuns` shows the watch *is* re-established on recreation, because `ensureConfigDirectoryWatched()` re-arms onto the nearest existing ancestor from the `directoryChanged` handler. Verified red: with the ancestor re-arm removed, the test fails (`store saves == 0`).
- [x] The user is grilled on whether to fix it and the approach, before any code changes. Superseded in practice: the verification found the edge already holds, so there was no fix (or approach) to choose and no production code changed.
- [ ] If acted on: deleting and recreating the config directory while the module runs re-establishes the watch, and subsequent edits to `mimeapps.list`/`kdeglobals`/the store trigger capture; pinned by a shell test with injected fakes (per ticket 08). Not applicable — the "acted on" (fix) branch was not taken; the behaviour already held and is pinned by the test regardless.
- [x] If declined: the decision and rationale are recorded in this ticket — **nothing to fix**: the current code already re-arms the watch onto the nearest surviving ancestor, so deleting and recreating the config directory is already handled. Pinned by a test (`9fcceaa`) instead of a fix; the stale ticket-10 note is corrected there (see review note below).

**Note from review of 25:** Reviewed at `9fcceaa`. Verdict: **approve**. The verification disproved the ticket-10 note rather than confirming it: the current module's `ensureConfigDirectoryWatched()` already walks up to the nearest existing ancestor and is re-entered from the `directoryChanged` handler, so a deleted/recreated config directory *is* re-watched within the same run. Verified red independently: with the ancestor re-arm removed, `configDirDeletedAndRecreatedIsRewatchedAndCaptureRuns` fails at `store saves == 0`; restored, it passes. No production change. `ctest` 7/7 with full deps; `clang-format` clean. The "If acted on" box is not applicable (no fix was needed). The stale ticket-10 note is corrected in ticket 10.

**Suggested skills:** `work-ticket` (implementer — verify the note and grill the user); `review-ticket` (reviewer).

**References:**
- `.scratch/activity-defaults-switcher/issues/10-watch-config-dir-when-created.md` — "Notes from review of 10".
- `src/module.cpp` — `ensureConfigWatched`, `upgradeConfigDirWatch`, `directoryChanged`.
- `tests/tst_module.cpp` — the shell tests (`configDirCreatedAfterStartupIsWatchedAndCaptureRuns`, `recreatedFileIsReWatchedAndCaptureRuns`).
- `.scratch/activity-defaults-switcher/issues/08-kded-shell-tests.md` — the injected-fakes seam.
