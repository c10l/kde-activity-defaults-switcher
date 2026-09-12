# 24: Close the KCM QML smoke test's coverage boundaries

**What to build:** Ticket 22's offscreen smoke test (`tests/kcm_qml/tst_kcm_qml.cpp`) loads `kcm/ui/main.qml` through a test-only copy of `kcm/kcm.cpp`'s `KQuickConfigModule` shell (`tests/kcm_qml/kcm_smoke_module.cpp`), and it only exercises the "activities exist" path. Ticket 22's review note recorded two coverage boundaries as accepted, with no follow-up ticket:

1. **Shell-wiring drift is not pinned.** Because the smoke shell is a copy, a change to `kcm/kcm.cpp`'s wiring (the `backend` property, the `org.kde.ads` enum registration, `load()`) would not fail the smoke test; only view drift is caught.
2. **The empty state is not exercised.** No test renders the "No activities to configure" state (ticket 07 AC 9, editing disabled).

**Verification first (grill the user):** Before writing code, verify both boundaries still hold against the current test sources, then interview the user on which (if either) to close and how — e.g. load the production `kcm` module class/plugin instead of the copy, and/or drive the smoke backend to zero activities and assert the empty-state label and disabled editing. Implement only after that.

**Blocked by:** 22 (offscreen QML smoke test), 07 (KCM component).

**Status:** ready-for-agent

- [ ] Both boundaries are verified against `tests/kcm_qml/` and `kcm/kcm.cpp`, with the findings reported.
- [ ] The user is grilled on which boundary to close and the approach, before any code changes.
- [ ] If acted on: each agreed boundary is closed and pinned by a test that fails when the relevant behaviour is broken (shell-wiring drift causes a smoke failure; zero activities renders the empty state and disables editing).
- [ ] If declined: the decision and rationale are recorded in this ticket.

**Suggested skills:** `work-ticket` (implementer — verify the note and grill the user); `review-ticket` (reviewer).

**References:**
- `.scratch/activity-defaults-switcher/issues/22-kcm-qml-smoke-test.md` — "Note from review of 22".
- `tests/kcm_qml/kcm_smoke_module.cpp`, `tests/kcm_qml/tst_kcm_qml.cpp` — the test shell and suite.
- `kcm/kcm.cpp` — the production shell whose wiring is copied.
- `kcm/ui/main.qml` — the empty-state label.
- `.scratch/activity-defaults-switcher/issues/07-kcm-component.md` — AC 9 (empty state).
