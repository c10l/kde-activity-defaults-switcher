# 21: KCM picker renders a non-enumerated binding as Inherit

**What to build:** When a Component's effective Application is not in the picker's enumerated candidate list — most visibly an **explicit** binding to an Application that is no longer installed — `componentPicker.syncSelection()` (`kcm/ui/main.qml`) finds no matching `storageId` and falls back to `currentIndex = 0`, the Inherit entry. The row then reads "Inherit" (or the baseline's label) for a Component that is actually bound, so the view stops showing the effective app (ticket 07 AC 2). Make the picker show the bound Application in that case. A **fallback** binding to a non-enumerated Application is partly covered today (the Inherit label names it via `ApplicationBackend::displayNameFor`), but should be consistent with the explicit case.

**Blocked by:** 07 (KCM component).

**Status:** done

- [x] An explicit binding whose Application is absent from `pickerEntries()` still shows that Application (its `displayNameFor` name, or at worst its storageId) as the row's effective app, not "Inherit".
- [x] A fallback binding whose Application is absent from the enumerated list is shown as fallback, consistently with the explicit case.
- [x] The behaviour is pinned by a backend test (the candidate list the picker is given includes the bound Application) and/or the QML smoke test from [22](22-kcm-qml-smoke-test.md); no free-text path is introduced.
- [x] `make format-check` and clang-tidy stay clean.

**Note from review of 21:** Reviewed at `bf1c35c`. `pickerEntries()` now appends the effective Application (via `displayNameFor`, which falls back to the storageId) after the enumerated candidates when it is not already present; `componentPicker.syncSelection()` then finds it and selects it, so the row shows the bound app instead of the Inherit entry. Both new tests fail on the pre-fix code and pass after. `ctest` is 6/6 with full deps and 5/5 in the deps-limited build; `make format-check` and clang-tidy are clean. No free-text path is added: the extra entry comes only from a stored explicit or fallback binding, and the fallback row keeps its italic `BindingSource::Fallback` styling. End-to-end rendering remains ticket 22. No findings.

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- `kcm/ui/main.qml` — `syncSelection()`.
- `src/kcm_backend.cpp` — `pickerEntries()`.
- `.scratch/activity-defaults-switcher/issues/07-kcm-component.md` — AC 2.
- `.scratch/activity-defaults-switcher/issues/22-kcm-qml-smoke-test.md`.
