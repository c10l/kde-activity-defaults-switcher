# 05: All components + profile store + GUI-change capture

**What to build:** The full component→mime map for all 13 components, the KConfig profile store (one group per component, activity id as key, storageId as value), and capture of System Settings edits into the current activity's profile via `KConfigWatcher`.

**Blocked by:** 04 (kded6 module — browser tracer bullet).

**Status:** done

- [x] All 13 components resolve and flip per activity.
- [x] Profiles persist in a KConfig file with the agreed structure.
- [x] Editing a default application in System Settings records a binding for the current activity.
- [x] Components whose resolved application already matches the current default are not rewritten (no churn).
- [x] `applyAndRemember` records every resolved decision in `m_lastDefaults` — including components absent from `currentDefaults` — so the daemon's own write is never later captured as a user edit. (Blocking, from review of 05: the `currentDefaults.contains(key)` guard drops a decision for a component with no prior mimeapps.list entry, so the next file-watcher event re-reads that default as a "user edit" and records a spurious binding into the current activity's profile — e.g. pinning a baseline binding into a child activity and breaking baseline fallback, user story 7.)

**Note (from review of 05):** The "all 13 components resolve and flip" criterion is met for the 12 mime components (writer) plus terminal *resolution* (resolver). The terminal's *flip* (kdeglobals) and terminal capture belong to ticket 06, per ticket 03 ("terminal is out of scope") and ticket 06.
