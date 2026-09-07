# 03: The C++ backend boundary

Type: grilling

## Question

What is the testable C++ backend for the KCM, and what does the thin QML consume?

- Which non-UI logics live in C++ (profile read/write via `ProfileStore`, app enumeration, baseline selection, effective-app computation, activity listing) vs QML?
- The interface exposed to QML: a `QAbstractListModel` of component rows (roles: component name, effective app, explicit-vs-fallback, baseline app)? `Q_PROPERTY`s? Which activity is selected?
- How the KCM enumerates activities and the baseline (reuse the daemon's `KActivitiesActivityBackend`/`KActivities::Consumer`, or a lighter reader) — and how the baseline activity is marked.
- The test seams: what's pure (effective-app), what's temp-dir (`ProfileStore`), what's injected (app enumeration behind an interface, activity backend) — mirroring the spec's Testing Decisions.

References: `src/domain.*` (`orderedActivityList`, `selectBaselineActivity`, `Profile`), `src/store.cpp`, `src/kactivities_backend.*`, `.scratch/activity-defaults-switcher/spec.md` (Testing Decisions), `CONTEXT.md`.
