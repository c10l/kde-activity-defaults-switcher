# 05: The picker's Inherit entry and clear semantics

Type: grilling
Status: resolved
Blocked by: 02, 03

## Question

How does the picker present clearing a binding, and how does clearing map onto `Profile::removeBinding`?

- Encode "Inherit (baseline: \<app\>)" as the first picker entry; decide the sentinel (empty string? a role flag?) and how selecting it removes the explicit binding so the component falls back to the baseline.
- How the row's effective app updates after a clear (explicit → fallback), and how a component with no explicit and no baseline binding shows as unset.
- The Inherit entry sits above the dynamically-enumerated apps from [02](02-app-picker-enumeration.md); no free-text storageId.

References: ticket 07 ACs 5–6, `src/domain.h` (`Profile::removeBinding`), ticket 02.

## Answer

Agreed with the maintainer. Four decisions:

**1. Picker candidate list composed by the backend.** The `Q_INVOKABLE` per component returns the complete ordered candidates: the Inherit entry first, then the enumerated apps from [02](02-app-picker-enumeration.md). Each entry carries `storageId` + `displayName` + an `isInheritEntry` flag. QML renders whatever it is given — no name resolution or list composition in QML (03 decision 1), and no baseline-app-name role on the row model.

**2. Sentinel.** The Inherit entry is `storageId == ""` (empty) **and** `isInheritEntry == true` (true only for that entry). Selecting it calls `Profile::removeBinding(component)`. Empty string stays the persisted "no application" sentinel (the store already skips it, the resolver ignores it); the flag disambiguates the picker entry and lets QML render it distinctly.

**3. Inherit label.** `"Inherit (baseline: <name>)"` when a baseline binding exists for the component, plain `"Inherit"` when none. The entry is always present, so clearing is always reachable — to fallback when a baseline binding exists, to unset when it does not.

**4. Display, selection, and row update.**
- Explicit: the closed combo shows the explicit app, and the dropdown highlights that app.
- Fallback: the closed combo shows the baseline app marked fallback, and the dropdown highlights the baseline's app (open and closed agree). Selecting the currently-effective fallback app promotes it to an explicit binding — a deliberate snapshot.
- Unset: the closed combo shows a placeholder (`"Inherit"`/`"Unset"`); the Inherit entry is the only item to highlight.
- The Inherit entry is always the first, clear action in the dropdown.
- After a clear: `Profile::removeBinding(component)` → recompute `effectiveBindings` (04) → the row's source flips explicit → fallback/unset and `dataChanged` fires. No extra machinery.

No new glossary term and no ADR: these are reversible UI details, and "unset" is already carried by 04's `BindingSource::Unset`.
