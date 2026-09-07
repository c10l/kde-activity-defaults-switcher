# 06: Save, Apply, Defaults and per-activity Reset

Type: grilling
Blocked by: 01

## Question

How do KQuickConfigModule's save/apply/defaults lifecycle and ticket 07's per-activity Reset map together?

- Save timing: does the KCM write `activity-defaults-switcherrc` on Apply (Save) or on every change? Ticket 07 says edits persist and a current-activity edit is applied live by the daemon (ticket 16); pick the timing that makes that hold.
- The built-in Apply and Defaults buttons: is "Defaults" the per-activity Reset (needs an activity selected) or a module-wide clear-all? Keep, suppress, or repurpose the built-in buttons to satisfy ticket 07's Reset (clears one activity's explicit bindings).
- How Reset clears a whole activity's explicit bindings via `ProfileStore` (per-component `removeBinding`), and how it stays distinct from the Inherit clear ([05](05-picker-inherit-entry-and-clear.md)).

References: ticket 07 ACs 4, 6–7, ADR-0002, [01](01-kcm-build-wiring-and-kquickconfigmodule-lifecycle.md), `src/store.cpp`.
