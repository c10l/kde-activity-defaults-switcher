# KCM (System Settings module) — wayfinding map

## Destination

A refined, implementation-ready spec for the KCM ([ticket 07](../activity-defaults-switcher/issues/07-kcm-component.md)): backend interface + roles, QML structure, build wiring, app-enumeration approach, and testing approach all locked — so the KCM can afterwards be built as a sequence of small work-ticket slices with nothing left to decide. The spec supersedes ticket 07's design section.

## Notes

- Domain: Activity Defaults Switcher. Use the `CONTEXT.md` vocabulary (Activity, Component, Binding, Profile, Application, Baseline activity; fallback binding / explicit binding) in all code, tickets, and answers.
- Skills per ticket type: `grilling` + `domain-modeling` for decisions, `research` for facts. Planning only — no KCM code is written under this map; implementation hands off to `work-ticket`.
- Already locked, do not re-litigate: ADR-0001 (the daemon flips system-wide defaults), ADR-0002 (KCM writes the profile store directly; the daemon watches and applies live), and reuse of the `ads-store`/`ads-domain`/`ads-writer` libraries.
- Tracker: local markdown (`.scratch/kcm-component/`).

## Decisions so far

- [01: KCM build wiring and the KQuickConfigModule lifecycle](issues/01-kcm-build-wiring-and-kquickconfigmodule-lifecycle.md): `find_package(KF6KCMUtils)` + `kcmutils_add_qml_kcm` (no `KDE_INSTALL_KCMDIR` in KF6); `load/save/defaults` virtuals, `needsSave`, `supportsInstantApply` (default true); new deps `libkf6kcmutils-dev`, `libkf6declarative-dev`, `kirigami2-dev`.
- [02: App picker enumeration](issues/02-app-picker-enumeration.md): mime via `queryByMimeType` (preference-sorted, `storageId()` is the written value); terminal via `KApplicationTrader::query` filtering `categories() == "TerminalEmulator"` (no `KServiceTypeTrader` in KF6).
- [03: The C++ backend boundary](issues/03-cpp-backend-boundary.md): C++ thinks, QML draws; one `KcmBackend` QObject + a `QAbstractListModel` of component rows (roles: name, effective app, explicit/fallback/unset, baseline app) + `selectedActivityId`/`baselineActivityId`/`currentActivityId`/`activities`/`hasActivities` properties; activities read via the daemon's `ActivityBackend`/`KActivitiesActivityBackend`, extracted into a shared `ads-activities` library; testing = pure effective-app in `ads-domain`, `ProfileStore` on a temporary directory, `ActivityBackend` + new `ApplicationBackend` as fakes.
- [04: Effective-app computation and baseline marking](issues/04-effective-app-computation-and-baseline-marking.md): new pure `effectiveBindings(selectedProfile, baselineProfile)` in `ads-domain` (explicit → fallback → unset; never captures) returning a `BindingSource` enum (`Explicit`/`Fallback`/`Unset`) + effective-app + baseline-app; baseline marked via `baselineActivityId`; current activity only defaults the selector, not visually marked.
- [05: The picker's Inherit entry and clear semantics](issues/05-picker-inherit-entry-and-clear.md): the `Q_INVOKABLE` per component returns the full ordered picker list — Inherit entry first, then the enumerated apps — each with `storageId` + `displayName` + `isInheritEntry`; Inherit = empty `storageId` + `isInheritEntry` flag, selecting it calls `Profile::removeBinding`; label "Inherit (baseline: \<name\>)" when a baseline binding exists, plain "Inherit" when none; the combo highlights the effective app for explicit/fallback (selecting a fallback app promotes it to explicit) and the Inherit entry when unset; a clear recomputes `effectiveBindings` and fires `dataChanged`.
- [06: Save, Apply, Defaults and per-activity Reset](issues/06-save-apply-defaults-reset.md): instant apply — the KCM writes the store on every edit (so the daemon's watch applies a current-activity edit live), `supportsInstantApply` true, `needsSave` never set, `save()` left as the base no-op; suppress the built-in Apply and Default buttons (`buttons` = `NoAdditionalButton`, no `defaults()`), per-activity Reset is KCM UI calling `resetActivity(activityId)` (per-component `removeBinding` + `ProfileStore::save()` + recompute `effectiveBindings` + `dataChanged`); distinct from the Inherit clear only by granularity — one component vs all components of one activity.

## Not yet specified

- KCM string translation (i18n) — in scope, not yet sharp.

## Out of scope

- Live refresh (ticket 07: snapshot on open, no live refresh).
- Free-text storageId in v1 (dynamic enumeration only).
- Any daemon/resolver/writer change (ticket 16 already delivered the daemon half of ADR-0002). The `ads-activities` library extraction in [03](issues/03-cpp-backend-boundary.md) is a deliberate exception: build-file-only, no daemon behaviour change.
- Distribution packaging.
- Per-mime-type overrides below the component level.
