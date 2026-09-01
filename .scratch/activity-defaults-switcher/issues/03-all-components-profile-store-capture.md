# 03: All components + profile store + GUI-change capture

**What to build:** The full component→mime map for all 13 components, the KConfig profile store (one group per component, activity name as key, storageId as value), and capture of System Settings edits into the current activity's profile via `KConfigWatcher`.

**Blocked by:** 02 (kded6 module — browser tracer bullet).

**Status:** ready-for-agent

- [ ] All 13 components resolve and flip per activity.
- [ ] Profiles persist in a KConfig file with the agreed structure.
- [ ] Editing a default application in System Settings records a binding for the current activity.
- [ ] Components whose resolved application already matches the current default are not rewritten (no churn).
