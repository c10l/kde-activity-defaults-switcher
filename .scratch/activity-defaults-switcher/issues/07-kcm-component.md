# 07: KCM (System Settings module)

**What to build:** A System Settings module (`KQuickConfigModule`, QML) to view, edit, and reset per-activity profiles. Design settled by grill; details below.

## Design

**Operations** (profiles only — never the global system defaults):

- **View:** browse every activity's bindings. Each component shows its effective app — an **explicit binding** when set, otherwise the baseline's binding marked as **fallback** — and the baseline activity is marked.
- **Edit:** set or clear a binding per component per activity. Setting picks from the dynamic app list; clearing = an **"Inherit (baseline: \<app\>)"** entry at the top of the picker.
- **Reset:** clear a whole activity's explicit bindings, so it falls back to the baseline (and keeps following future baseline changes).

**Layout:** activity selector at the top; below, a scrollable list of the 13 components, each row = component name + effective-app combo. Empty state: "No activities to configure", editing disabled.

**App picker:** dynamic enumeration only (no free-text storageId in v1). The 12 mime components via `KApplicationTrader::queryByMimeType(primaryMimeType)` (preference-sorted; Flatpak included automatically). The terminal via the `TerminalEmulator` category filter.

**Data/apply path ([ADR 0002](../../../docs/adr/0002-kcm-writes-profile-store-daemon-watches.md)):** the KCM links `ads-store`/`ads-domain` and reads/writes `activity-defaults-switcherrc` directly. The daemon watches the store ([ticket 16](16-daemon-watches-profile-store.md)) and re-applies, so an edit to the *current* activity flips live; edits to other activities persist until that activity becomes current. The KCM reads on open (snapshot, no live refresh).

**Blocked by:** 06 (Terminal component + baseline fallback), 16 (daemon watches the profile store).

**Status:** done

- [x] The KCM is a QML `KQuickConfigModule` registered in System Settings (new build deps: kcmutils, kdeclarative, etc.).
- [x] View: every activity is listed; each component shows its effective app, with explicit vs fallback bindings visually distinguished and the baseline activity marked.
- [x] View: a component with no explicit binding and no baseline binding shows as unset.
- [x] Edit: setting a binding persists it through `ProfileStore` in the exact structure the daemon reads (`activity-defaults-switcherrc`, one group per component, activity id as key, storageId as value).
- [x] Edit: the picker offers the dynamic app list per component (mime query / `TerminalEmulator`), Flatpak included, no free-text entry.
- [x] Edit: clearing via the "Inherit (baseline: \<app\>)" entry removes the explicit binding, so the component falls back to the baseline.
- [x] Reset: resetting an activity clears all its explicit bindings, so it falls back to the baseline (and follows future baseline changes).
- [x] Apply path: an edit to the current activity's binding is applied live by the daemon (via ticket 16); an edit to a non-current activity persists and applies when that activity becomes current.
- [x] Empty state: no activities → "No activities to configure", editing disabled.
- [x] The KCM's non-UI logic (profile read/write, app enumeration, baseline selection, effective-app computation) is a testable C++ backend with tests; the QML is thin.
- [x] `ctest` (or the project's test command) passes.

**Note from review of 07:** Reviewed at `846c909`. `ctest` is 6/6 with full deps (resolver, writer, switcher, store, kcm, module) and 5/5 in the deps-limited build; clang-format and clang-tidy are clean on the new sources. The QML could only be linted, not rendered, in review: `qmllint` reports only the expected unqualified-access false positives for the runtime-injected `kcm`/`i18n`/`org.kde.ads`, no syntax or type errors. Two non-blocking findings are tracked in new tickets: **21** (an explicit binding whose Application is not in the enumerated list renders as "Inherit") and **22** (no offscreen QML smoke test). Design ticket 06.1's "set `supportsInstantApply` explicitly for clarity" is not done, but the base default is `true`, so the behaviour is correct.

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- [ADR 0002](../../../docs/adr/0002-kcm-writes-profile-store-daemon-watches.md) — the write/apply path.
- [Ticket 16](16-daemon-watches-profile-store.md) — the daemon's store-watch half.
- `.scratch/activity-defaults-switcher/spec.md` — *Implementation Decisions* (profile store structure) and *Components*.
- `src/store.cpp`, `src/domain.cpp` (`selectBaselineActivity`, `orderedActivityList`), `src/writer.cpp` (`primaryMimeTypeFor`).
- `CONTEXT.md` — vocabulary (including **fallback binding** / **explicit binding**).
