# 05: The picker's Inherit entry and clear semantics

Type: grilling
Blocked by: 02, 03

## Question

How does the picker present clearing a binding, and how does clearing map onto `Profile::removeBinding`?

- Encode "Inherit (baseline: \<app\>)" as the first picker entry; decide the sentinel (empty string? a role flag?) and how selecting it removes the explicit binding so the component falls back to the baseline.
- How the row's effective app updates after a clear (explicit → fallback), and how a component with no explicit and no baseline binding shows as unset.
- The Inherit entry sits above the dynamically-enumerated apps from [02](02-app-picker-enumeration.md); no free-text storageId.

References: ticket 07 ACs 5–6, `src/domain.h` (`Profile::removeBinding`), ticket 02.
