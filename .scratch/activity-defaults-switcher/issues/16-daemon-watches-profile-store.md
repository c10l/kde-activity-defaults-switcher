# 16: Daemon watches the profile store (KCM edits apply live)

**What to build:** The daemon (`ActivityDefaultsSwitcherModule`, `src/module.cpp`) must watch the profile store (`activity-defaults-switcherrc`) alongside `mimeapps.list` and `kdeglobals`, and re-resolve/applies when it changes. This is the daemon-side half of [ADR 0002](../../../docs/adr/0002-kcm-writes-profile-store-daemon-watches.md): the KCM (ticket 07) writes the store directly, and the daemon notices and flips the live defaults so an edit to the current activity applies immediately.

**Decision ([ADR 0002](../../../docs/adr/0002-kcm-writes-profile-store-daemon-watches.md)):** KCM writes the profile store directly; the daemon watches it and applies live. Provisional — revisit the D-Bus option if the two-writer race materialises (see the ADR's *Revisit*).

**Blocked by:** 08 (kded shell tests) — the module injection seam needed to pin the watch-and-apply glue without KDE, D-Bus, or filesystem access.

**Status:** done

- [x] The module adds `activity-defaults-switcherrc` to its `QFileSystemWatcher` (file and directory, like the existing `mimeapps.list`/`kdeglobals` handling), so a created or recreated store is picked up too.
- [x] A change to the store, after the existing coalescing, triggers a re-resolve and apply — the same `refresh()` path as an activity change, not the capture path.
- [x] The daemon's own store writes (System Settings capture) do not cause a spurious re-apply loop; only a genuine external change (a KCM save) triggers apply.
- [x] Shell tests pin this behaviour with injected fakes (per ticket 08).

**Note from review of 16:** AC3 holds because `refresh()` never persists the store — a daemon-owned capture write settles after at most one idempotent re-apply, and only a genuinely external (KCM) store change ends up changing defaults. The module does not attempt to distinguish its own writes from KCM saves; it relies on that store-idempotency. The full feedback loop becomes assertable end-to-end once ticket 17 (single `ProfileStore`) removes the loader/saver duality.

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- [ADR 0002](../../../docs/adr/0002-kcm-writes-profile-store-daemon-watches.md) — the decision this implements.
- [Ticket 07 — KCM (System Settings module)](07-kcm-component.md) — the KCM half of the decision.
- `src/module.cpp` (`ensureConfigWatched`, watcher wiring, `refresh`), `src/store.cpp`.
- `CONTEXT.md` — vocabulary.
