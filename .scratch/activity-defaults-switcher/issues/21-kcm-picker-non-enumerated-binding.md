# 21: KCM picker renders a non-enumerated binding as Inherit

**What to build:** When a Component's effective Application is not in the picker's enumerated candidate list — most visibly an **explicit** binding to an Application that is no longer installed — `componentPicker.syncSelection()` (`kcm/ui/main.qml`) finds no matching `storageId` and falls back to `currentIndex = 0`, the Inherit entry. The row then reads "Inherit" (or the baseline's label) for a Component that is actually bound, so the view stops showing the effective app (ticket 07 AC 2). Make the picker show the bound Application in that case. A **fallback** binding to a non-enumerated Application is partly covered today (the Inherit label names it via `ApplicationBackend::displayNameFor`), but should be consistent with the explicit case.

**Blocked by:** 07 (KCM component).

**Status:** ready-for-agent

- [ ] An explicit binding whose Application is absent from `pickerEntries()` still shows that Application (its `displayNameFor` name, or at worst its storageId) as the row's effective app, not "Inherit".
- [ ] A fallback binding whose Application is absent from the enumerated list is shown as fallback, consistently with the explicit case.
- [ ] The behaviour is pinned by a backend test (the candidate list the picker is given includes the bound Application) and/or the QML smoke test from [22](22-kcm-qml-smoke-test.md); no free-text path is introduced.
- [ ] `make format-check` and clang-tidy stay clean.

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- `kcm/ui/main.qml` — `syncSelection()`.
- `src/kcm_backend.cpp` — `pickerEntries()`.
- `.scratch/activity-defaults-switcher/issues/07-kcm-component.md` — AC 2.
- `.scratch/activity-defaults-switcher/issues/22-kcm-qml-smoke-test.md`.
