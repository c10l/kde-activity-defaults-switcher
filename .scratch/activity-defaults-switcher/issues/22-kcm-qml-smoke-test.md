# 22: Offscreen smoke test for the KCM QML

**What to build:** Ticket 07's non-UI logic is covered by `tst_kcm`, but nothing exercises `kcm/ui/main.qml` at runtime — the implementer and the review could only lint it, not render it, so a QML regression (broken id, wrong role, unresolved enum, delegate error, mis-selected combo) ships silently. Add a test that instantiates the QML against a fake/real `KcmBackend` with `QT_QPA_PLATFORM=offscreen` and fails on any QML warning or error, so the view is pinned the way the backend already is.

**Blocked by:** 07 (KCM component).

**Status:** done

- [x] A ctest target loads `kcm/ui/main.qml` (with the KCMUtils/Kirigami QML modules and a `QQmlEngine`) offscreen with a fake/real `KcmBackend`, and fails the test on any QML warning or error.
- [x] The smoke test covers the three row states (explicit, fallback, unset) and the baseline activity badge, asserting the rendered ComboBox selection/label matches the backend's effective binding.
- [x] The target is built only when the KCMUtils/QML modules are present (the existing optional-target pattern), so the deps-limited build still configures and passes.
- [x] `make format-check` and clang-tidy stay clean.

**Note from review of 22:** Reviewed at `2446032`. `ctest` is 7/7 with full deps (`build-smoke`, `kcm-qml` included) and 5/5 in a freshly-configured deps-limited build (no `kcm-qml` target); `make format-check` is clean and clang-tidy is clean on the three new sources (`kcm_smoke_module.cpp`, `kcm_smoke_support.cpp`, `tst_kcm_qml.cpp`). The suite is genuinely red, not just green: injecting an unresolved import fails `viewLoadsWithoutWarningsOrErrors`; a non-fatal runtime `ReferenceError` (undefined identifier in a delegate binding) fails it while the view still loads, proving the whole-suite warning capture; and breaking `componentPicker`'s `textRole` makes the three row-state tests fail on the rendered label. Two coverage boundaries are accepted as out of this ticket's ACs and noted for the record: the smoke shell is a test-only copy of `kcm/kcm.cpp`'s wiring, so shell-wiring drift (as opposed to view drift) is not pinned; and the empty state (`No activities to configure`) is not exercised. No production code changed, and the ticket's Status and boxes were left for review, as required.

**Suggested skills:** `tdd`, `work-ticket` (implementer); `review-ticket` (reviewer).

**References:**
- `kcm/ui/main.qml`, `kcm/kcm.cpp` — the QML shell and its `KcmBackend`.
- `tests/tst_kcm.cpp` — the backend tests the smoke test complements.
- `.scratch/activity-defaults-switcher/issues/07-kcm-component.md` — AC 2, 3, 9.
