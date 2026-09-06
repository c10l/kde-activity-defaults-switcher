# 07: KCM (System Settings module)

**What to build:** A System Settings module (`KQuickConfigModule`) to view, edit, and reset per-activity profiles. Design not yet settled — this ticket is a placeholder pending a future grill session.

When the user asks to work on this issue, use the grill-with-docs skill to determine the scope, design and implementation details.

**Design note (grill, round 1):** Write path settled — the KCM writes the profile store directly and the daemon watches the store to apply edits live ([ADR 0002](../../../docs/adr/0002-kcm-writes-profile-store-daemon-watches.md)). The daemon half is [ticket 16](16-daemon-watches-profile-store.md); this ticket's remaining scope/design is still being grilled (round 2 pending).

**Blocked by:** 06 (Terminal component + baseline fallback), 16 (daemon watches the profile store), and a future grill session for its design.

**Status:** blocked — pending design (do not start)

- [ ] To be defined by the future grill session.
