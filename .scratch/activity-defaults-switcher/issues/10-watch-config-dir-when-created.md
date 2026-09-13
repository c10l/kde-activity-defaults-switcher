# 10: Watch the config directory when it appears after startup

**What to build:** Make `ActivityDefaultsSwitcherModule` still observe `mimeapps.list`/`kdeglobals` when the config directory does not exist at construction time. Today the directory is added to `QFileSystemWatcher` only when `QFile::exists(configDir)` at startup, and `ensureConfigWatched()` skips non-existent files — so if the directory is created later (fresh/blank config), no watcher is ever installed and System Settings edits are never captured (spec user story 6).

**Blocked by:** 08 (kded shell tests).

**Status:** done

- [x] The config directory is watched (or otherwise re-checked) even when absent at construction, so it is picked up once created.
- [x] `mimeapps.list`/`kdeglobals` created after startup are watched, and edits to them trigger capture.
- [x] Shell tests pin this with injected fakes (per ticket 08).

### Notes from review of 10

- The commit message's "self-heals across directory deletions/recreations" reads broader than the code guarantees: once the watch is upgraded onto the config directory itself, deleting that directory while the module runs makes `QFileSystemWatcher` silently drop the inotify watch, and no directory event fires on recreation because the nearest ancestor is no longer watched — so the watch is only re-established at the next daemon start. This is out of ticket 10's scope (the ticket is about a config dir absent at *construction*) and practically nil in impact (deleting `~/.config` under a live session breaks the session anyway, and login re-watches). Accepted as deferred; no follow-up ticket.
- **Correction from review of 25 (`9fcceaa`):** the note above is wrong for the current code. `ensureConfigDirectoryWatched()` is re-entered from the `directoryChanged` handler and, when the config directory vanishes, re-arms the watch onto the nearest existing ancestor — so recreating the directory fires an ancestor event, the watch is upgraded back onto the config dir, and later `mimeapps.list` edits are still captured. Pinned by `configDirDeletedAndRecreatedIsRewatchedAndCaptureRuns` in `tests/tst_module.cpp`.

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- `src/module.cpp` — constructor, `ensureConfigWatched()`, the `directoryChanged` handler.
- `.scratch/activity-defaults-switcher/spec.md` — user story 6 and the change-detection decision.
- `CONTEXT.md` — vocabulary.
