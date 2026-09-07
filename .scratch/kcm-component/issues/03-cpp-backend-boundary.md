# 03: The C++ backend boundary

Type: grilling
Status: resolved

## Question

What is the testable C++ backend for the KCM, and what does the thin QML consume?

- Which non-UI logics live in C++ (profile read/write via `ProfileStore`, app enumeration, baseline selection, effective-app computation, activity listing) vs QML?
- The interface exposed to QML: a `QAbstractListModel` of component rows (roles: component name, effective app, explicit-vs-fallback, baseline app)? `Q_PROPERTY`s? Which activity is selected?
- How the KCM enumerates activities and the baseline (reuse the daemon's `KActivitiesActivityBackend`/`KActivities::Consumer`, or a lighter reader) — and how the baseline activity is marked.
- The testing approach: what's pure (effective-app), what's temp-dir (`ProfileStore`), what's a fake passed in (app enumeration behind an interface, activity backend) — mirroring the spec's Testing Decisions.

References: `src/domain.*` (`orderedActivityList`, `selectBaselineActivity`, `Profile`), `src/store.cpp`, `src/kactivities_backend.*`, `.scratch/activity-defaults-switcher/spec.md` (Testing Decisions), `CONTEXT.md`.

## Answer

Agreed with the maintainer. Four decisions:

**1. Boundary: C++ thinks, QML draws.** All non-visual logic lives in C++: profile read/write (`ProfileStore`), application enumeration, baseline selection, effective-app computation, and the edit intents (set a binding, clear a binding, reset an activity). QML only renders the activity selector, the component rows, and the picker, and forwards user gestures to the backend. No decisions in QML.

**2. Interface exposed to QML.** One QML-visible `KcmBackend` QObject carrying:
- `selectedActivityId` — which activity's rows the model shows;
- `baselineActivityId` — so QML marks the baseline;
- `currentActivityId` — so the selector can default to it (whether to mark it *visually* is 04's call);
- `activities` — the ordered activity list (id + name) for the selector;
- `hasActivities` — the empty state;
- one `QAbstractListModel` of component rows for the selected activity, with roles: component name, effective app storageId, binding source (explicit / fallback / unset — the *encoding* is decided in 04), baseline app storageId;
- a `Q_INVOKABLE` per component returning the picker's candidate applications, enumerated through the `ApplicationBackend` interface.

**3. Activity enumeration and baseline.** Reuse the daemon's `ActivityBackend` interface and its `KActivitiesActivityBackend` reader, moved into a shared library (e.g. `ads-activities`) that both the daemon and the KCM link. This edits the daemon's build files but changes no behaviour — a deliberate exception to the map's "no daemon change" out-of-scope line, approved by the maintainer. The baseline is `Resolver::selectBaselineActivity` over the named activities — the same rule the daemon uses — with the nameless-activity guard the daemon applies in `module.cpp` applied KCM-side. The baseline is marked via `baselineActivityId` (decision 2).

**4. Testing approach.** Pure: the effective-app computation in `ads-domain` (no KDE, D-Bus, or filesystem) — its exact location is pinned by 04. Temporary directory: `ProfileStore` (reuse `ads-store`). Fakes: `ActivityBackend` and a new `ApplicationBackend` interface wrapping `KApplicationTrader::queryByMimeType` and the `TerminalEmulator` filter. The backend (the model, `KcmBackend`, `ApplicationBackend`) is its own static library `ads-kcm`, unit-tested with fakes; the `kcmutils_add_qml_kcm` plugin is a thin shell over it.
