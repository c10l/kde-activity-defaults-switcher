# 23: KCM — switch to Apply-button semantics

**Supersedes** the original scope of this ticket ("set `supportsInstantApply` explicitly for clarity"). Verification of that narrower note is recorded below; the user then decided the KCM should behave like the rest of System Settings — **the user clicks Apply to save** — which reverses the instant-apply decision in `.scratch/kcm-component/issues/06-save-apply-defaults-reset.md` decision 1.

## What to build

The KCM currently applies instantly: every edit in `KcmBackend` mutates the in-memory profile map and calls `ProfileStore::save()` immediately, and `ActivityDefaultsSwitcherKcm` relies on `KAbstractConfigModule`'s default `supportsInstantApply == true` with no Apply button.

Change it to Apply-button semantics:

- Edits (set binding, Inherit clear, per-activity Reset) mutate the in-memory `Profile` maps only. No `ProfileStore::save()` on edit.
- The module tracks pending changes: call `setNeedsSave(true)` on the first unsaved edit (base `needsSave()` is already `false`), and expose the built-in Apply button (`buttons` includes `Apply`).
- `save()` is overridden to persist the pending profiles once (`ProfileStore::save()`) and clear `needsSave` (`setNeedsSave(false)`).
- `supportsInstantApply` is set explicitly to `false` in `ActivityDefaultsSwitcherKcm`, with a comment stating the Apply-button contract (this is the originally requested explicit pin, now with the opposite value).
- `load()` discards any pending edits and re-reads the store (the shell calls `load()` on Reset/undo).

The daemon is unchanged. It still watches the store file and flips live on change ([ADR 0002](../../../docs/adr/0002-kcm-writes-profile-store-daemon-watches.md)); the change is only *when* the KCM writes the file — on Apply rather than on every edit. ADR-0002 still holds.

## Verification of the original note (AC 1, done)

- `supportsInstantApply` is **not set** in `kcm/kcm.cpp`.
- `KQuickConfigModule`'s effective default is **`true`** — confirmed against the installed `libKF6KCMUtilsCore.so.6.28.0`: the `KAbstractConfigModule` constructor sets the byte at private offset `0x3c` to `1`, and `supportsInstantApply()` reads that byte.
- `buttons` is already pinned, in the view: `kcm/ui/main.qml:24` sets `KCMUtils.ConfigModule.buttons: KCMUtils.ConfigModule.NoAdditionalButton`.
- No test instantiates `ActivityDefaultsSwitcherKcm`; the `tests/tst_kcm.cpp` seam is `KcmBackend`.

## Decision record

The user directed: *"Write down explicitly that we want to behave the same way as the rest of the system settings pages - user clicks apply to save."* This is a deliberate reversal of `.scratch/kcm-component/issues/06-save-apply-defaults-reset.md` decision 1 (instant apply), which the user stated they had not consciously chosen. The status of that design ticket's decision 1 is now **superseded by this ticket**.

## Blocked by

07 (KCM component) — done.

**Status:** ready-for-agent

- [ ] Editing a binding no longer writes the store; the store changes only on `save()`.
- [ ] The shell shows an Apply button and it is enabled after an edit (`setNeedsSave(true)` on first unsaved edit).
- [ ] Clicking Apply persists the pending profiles and clears `needsSave`.
- [ ] `supportsInstantApply` is set explicitly to `false` in `ActivityDefaultsSwitcherKcm`, with a comment stating the Apply-button contract.
- [ ] `load()` discards pending edits and re-reads the store.
- [ ] Covered by tests at the `KcmBackend` seam (pending-vs-persisted store writes, needsSave transitions); if any part has no seam, the rationale is recorded here.
- [ ] The `buttons` value in `kcm/ui/main.qml` is reconciled with the Apply decision (explicit Apply; no Default button).

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- `kcm/kcm.cpp` — the module constructor and `load()`; add `save()` and the explicit flag.
- `src/kcm_backend.cpp` / `kcm_backend.h` — the edit path that must stop saving eagerly.
- `.scratch/kcm-component/issues/06-save-apply-defaults-reset.md` — decision 1, now superseded.
- `.scratch/kcm-component/issues/01-kcm-build-wiring-and-kquickconfigmodule-lifecycle.md` — the lifecycle virtuals.
- `kcm/ui/main.qml` — the view's `buttons` line.
