# 06: Save, Apply, Defaults and per-activity Reset

Type: grilling
Status: resolved
Blocked by: 01

## Question

How do KQuickConfigModule's save/apply/defaults lifecycle and ticket 07's per-activity Reset map together?

- Save timing: does the KCM write `activity-defaults-switcherrc` on Apply (Save) or on every change? Ticket 07 says edits persist and a current-activity edit is applied live by the daemon (ticket 16); pick the timing that makes that hold.
- The built-in Apply and Defaults buttons: is "Defaults" the per-activity Reset (needs an activity selected) or a module-wide clear-all? Keep, suppress, or repurpose the built-in buttons to satisfy ticket 07's Reset (clears one activity's explicit bindings).
- How Reset clears a whole activity's explicit bindings via `ProfileStore` (per-component `removeBinding`), and how it stays distinct from the Inherit clear ([05](05-picker-inherit-entry-and-clear.md)).

References: ticket 07 ACs 4, 6–7, ADR-0002, [01](01-kcm-build-wiring-and-kquickconfigmodule-lifecycle.md), `src/store.cpp`.

## Answer

Three decisions, settled against ADR-0002 (the KCM writes the store; the daemon watches and applies live) and the ticket 01 lifecycle facts:

**1. Save timing: write the store on every edit (instant apply).** The KCM persists `activity-defaults-switcherrc` immediately on every edit, not on an Apply click. Only then does the daemon's store watch (ticket 16) fire on a current-activity edit and flip the system default live; deferring the write to Apply would leave the edit invisible to the daemon until the user clicks Apply, breaking AC 8's "applied live". Concretely: `supportsInstantApply` stays `true` (the `KAbstractConfigModule` default — set it explicitly for clarity); every edit intent (set a binding, Inherit clear, Reset) mutates the in-memory profile map and calls `ProfileStore::save()` right away; `setNeedsSave(true)` is never called, so the shell offers no Apply/OK (just Close). `save()` is left as the base no-op — nothing is ever pending. `load()` re-reads the store and rebuilds the model; the shell's own Reset (undo) calls it and becomes a harmless re-read.

**2. Buttons: suppress Apply and Default; per-activity Reset is KCM UI, not `defaults()`.** The built-in Default button drives `defaults()`, which is module-wide ("set configuration to default values") — it does not match ticket 07's per-activity Reset, and a module-wide clear-all is out of scope. So `defaults()` is not used and the Default button is suppressed (excluded from `buttons`). The Apply button is likewise excluded (nothing to flush; instant apply). `buttons` = `NoAdditionalButton` (optionally `| Help`). The per-activity Reset is an explicit button in the KCM's selected-activity view that calls a backend `resetActivity(activityId)`; it is disabled in the empty state like the rest of editing.

**3. Reset = bulk per-activity clear through the same `removeBinding` path.** `KcmBackend::resetActivity(activityId)` (Q_INVOKABLE) removes every binding of that activity: for each `Component` in `allComponents()`, `profile.removeBinding(component)`, then `ProfileStore::save()` (instant apply), then recompute `effectiveBindings` and fire `dataChanged` for all rows — every component reverts to fallback (when the baseline has a binding) or unset. It stays distinct from the Inherit clear ([05](05-picker-inherit-entry-and-clear.md)): Inherit clears *one component's* binding via the picker entry, Reset clears *all of one activity's* bindings in one action; both funnel through `Profile::removeBinding` + `ProfileStore::save`, so the persisted shape is identical and only the granularity differs. Reset requires an activity selected; resetting the baseline activity is allowed and simply leaves it unset (it has no baseline of its own — the daemon's capture re-establishes bindings at apply time), so no special-casing. Optionally the button can be disabled when the selected profile has no explicit bindings (`Profile::isEmpty()`), a UX nicety not required by AC 7.
