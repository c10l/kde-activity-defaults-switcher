# 22: Offscreen smoke test for the KCM QML

**What to build:** Ticket 07's non-UI logic is covered by `tst_kcm`, but nothing exercises `kcm/ui/main.qml` at runtime — the implementer and the review could only lint it, not render it, so a QML regression (broken id, wrong role, unresolved enum, delegate error, mis-selected combo) ships silently. Add a test that instantiates the QML against a fake/real `KcmBackend` with `QT_QPA_PLATFORM=offscreen` and fails on any QML warning or error, so the view is pinned the way the backend already is.

**Blocked by:** 07 (KCM component).

**Status:** ready-for-agent

- [ ] A ctest target loads `kcm/ui/main.qml` (with the KCMUtils/Kirigami QML modules and a `QQmlEngine`) offscreen with a fake/real `KcmBackend`, and fails the test on any QML warning or error.
- [ ] The smoke test covers the three row states (explicit, fallback, unset) and the baseline activity badge, asserting the rendered ComboBox selection/label matches the backend's effective binding.
- [ ] The target is built only when the KCMUtils/QML modules are present (the existing optional-target pattern), so the deps-limited build still configures and passes.
- [ ] `make format-check` and clang-tidy stay clean.

**Suggested skills:** `tdd`, `work-ticket` (implementer); `review-ticket` (reviewer).

**References:**
- `kcm/ui/main.qml`, `kcm/kcm.cpp` — the QML shell and its `KcmBackend`.
- `tests/tst_kcm.cpp` — the backend tests the smoke test complements.
- `.scratch/activity-defaults-switcher/issues/07-kcm-component.md` — AC 2, 3, 9.
