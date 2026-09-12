# 23: KCM — set `supportsInstantApply` explicitly

**What to build:** `ActivityDefaultsSwitcherKcm` (`kcm/kcm.cpp`) never sets `supportsInstantApply`; it relies on `KAbstractConfigModule`'s default (`true`), which is the instant-apply behaviour ticket 07 wants and which the design for the save/apply path (`.scratch/kcm-component/issues/06-save-apply-defaults-reset.md`, decision 1) asked to be set "explicitly for clarity". Ticket 07's review note recorded that this clarity item was not done, but that the behaviour is correct because the base default is `true`. This is a robustness/clarity item, not a bug: an upstream default change or a later edit could silently flip the KCM to Apply-button semantics.

**Verification first (grill the user):** Before writing code, verify the note against the current tree — confirm `supportsInstantApply` is unset in `kcm/kcm.cpp` and what `KQuickConfigModule`'s default is — then interview the user on whether to pin it explicitly (and whether the neighbouring lifecycle knobs — `needsSave`, `save()`, `buttons` — should be pinned at the same time, and how it should be tested). Implement only after that.

**Blocked by:** 07 (KCM component).

**Status:** ready-for-agent

- [ ] The note is verified against `kcm/kcm.cpp` and `KQuickConfigModule` (unset here; its effective value; whether explicit setting is desirable), with the findings reported.
- [ ] The user is grilled on whether and how to act, before any code changes.
- [ ] If acted on: `supportsInstantApply` is set explicitly in `ActivityDefaultsSwitcherKcm` with a comment stating the instant-apply contract, and any agreed neighbouring knobs are pinned the same way; the choice is covered by a test if a seam exists, otherwise the rationale is recorded in this ticket.
- [ ] If declined: the decision and its rationale are recorded in this ticket.

**Suggested skills:** `work-ticket` (implementer — verify the note and grill the user); `review-ticket` (reviewer).

**References:**
- `kcm/kcm.cpp` — the module constructor and `load()`.
- `.scratch/kcm-component/issues/06-save-apply-defaults-reset.md` — decision 1 (instant apply; "set it explicitly for clarity").
- `.scratch/activity-defaults-switcher/issues/07-kcm-component.md` — "Note from review of 07".
- `.scratch/kcm-component/issues/01-kcm-build-wiring-and-kquickconfigmodule-lifecycle.md` — the lifecycle virtuals and the `supportsInstantApply` default.
- `kcm/ui/main.qml` — the view that assumes instant apply.
