# 08: kded shell tests (inject fakes + cover the module glue)

**What to build:** Make `ActivityDefaultsSwitcherModule` (`src/module.cpp`, `src/module.h`) testable by injecting its dependencies — `Resolver`, `Writer`, `ProfileStore`, and any seam needed to drive the `KActivities::Consumer` and `QFileSystemWatcher` — then add tests for the shell glue at the agreed shell seam (spec *Testing Decisions*). The module is currently the only untested layer: every non-trivial wiring decision (signal connections, the 100 ms coalescing timer, the directory-watch re-add logic, startup refresh) is unprotected.

**Why (from review):** The spec explicitly requires "the kded shell is tested with a fake resolver/writer injected", but no such tests exist. The domain/writer/switcher/store layers are well covered; the shell is the gap.

**Blocked by:** 06 (Terminal component + baseline fallback).

**Status:** ready-for-agent

- [ ] The module accepts injected `Resolver`/`Writer`/`ProfileStore` (and any other seam) so tests never touch KDE, D-Bus, or the filesystem.
- [ ] A shell test covers: current-activity change calls `Switcher::switchActivity` with the resolved current/baseline/profiles/defaults.
- [ ] A shell test covers: a config-file change (after coalescing) calls `Switcher::captureUserEdits`.
- [ ] A shell test covers: the 100 ms timer coalesces rapid file-change events into a single capture.
- [ ] A shell test covers: a recreated watched file is re-added to the watcher and triggers a capture.
- [ ] `recreatedFileIsReWatchedAndCaptureRuns` must delete and recreate the watched file, not rewrite it in place: a `QIODevice::Truncate` rewrite keeps the same inode, so the `directoryChanged` → re-add branch never runs (verified by disabling that handler — the test still passes). As written it does not cover "re-added to the watcher".
- [ ] A shell test covers: startup `refresh()` when the KActivities backend is already `Running`, and `KActivities::Consumer::serviceStatus` changing to `Running` triggers a refresh.
- [ ] A shell test covers: no refresh/capture happens while the KActivities backend is not `Running`.
- [ ] Update `.scratch/activity-defaults-switcher/spec.md` so its change-detection wording says `QFileSystemWatcher` (not `KConfigWatcher`), matching the implementation and the rationale already documented in `src/module.cpp`.

### Notes from review of 08

- Criterion 2's "profiles/defaults" half is unasserted: the shell tests record `lastProfiles`/`lastDefaults` in the fake resolver but never check what the module passed.
- The module keeps a real `ProfileStore m_store` for loads while the injected store only serves saves (its `load()` is never exercised in `tst_module`); production runs two store instances against the same file. Consider routing loads through the injected store too.

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- `.scratch/activity-defaults-switcher/spec.md` — *Testing Decisions* and *Implementation Decisions* (change detection).
- `.scratch/activity-defaults-switcher/issues/04-kded-browser-tracer-bullet.md` — the module's original tracer-bullet scope.
- `src/module.cpp`, `src/module.h`, `src/switcher.h`.
- `CONTEXT.md` — domain vocabulary to use in code and tests.
