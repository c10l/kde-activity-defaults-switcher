# 09: React to activity renames + guard transient empty names

**What to build:** Two fixes to baseline selection in `ActivityDefaultsSwitcherModule` (`src/module.cpp`):

1. **Observe renames.** The module listens to `KActivities::Consumer::currentActivityChanged`, `activitiesChanged`, and `serviceStatusChanged`, but not `KActivities::Info::nameChanged`. `activitiesChanged` fires on add/remove, not on rename. So renaming an activity to/from `"Default"` (or any rename that changes the case-insensitive fallback order) does not re-resolve the baseline until some unrelated event happens. This breaks spec user story 8 and the ticket-02 premise that names are user-renamable.
2. **Guard transient empty names.** `allActivities()` builds `Activity{id, activityName(id)}`, and `activityName()` lazily creates a `KActivities::Info`. Per the KActivities docs (`deps/usr/include/PlasmaActivities/plasmaactivities/info.h`), a freshly-created `Info` can return an empty name before it synchronizes. `orderedActivityList()` sorts empty names first, so a just-added activity could transiently become the fallback baseline when no `"Default"` exists.

**Blocked by:** 08 (kded shell tests) — the module needs the injection seam to test these behaviours.

**Status:** done

- [x] Renaming an activity triggers a refresh that re-selects the baseline (e.g. listen to `KActivities::Info::nameChanged`, or re-query names on the relevant signal).
- [x] Renaming an activity to `"Default"` makes it the baseline.
- [x] Renaming `"Default"` away from that name falls back to the first activity in the ordered list.
- [x] A transient empty name from a not-yet-synchronized `Info` cannot mis-select the baseline (ignore or re-query empty names).
- [x] Shell tests pin these behaviours with injected fakes (per ticket 08).

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- `.scratch/activity-defaults-switcher/spec.md` — user story 8 and the *Baseline activity selection* decision.
- `.scratch/activity-defaults-switcher/issues/02-activity-identity.md` — "names are user-renamable".
- `src/module.cpp` (`activityName`, `allActivities`, `refresh`), `src/domain.cpp` (`orderedActivityList`, `selectBaselineActivity`).
- `deps/usr/include/PlasmaActivities/plasmaactivities/info.h` — the empty-name note.
- `CONTEXT.md` — vocabulary.
