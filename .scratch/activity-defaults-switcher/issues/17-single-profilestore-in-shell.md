# 17: Single ProfileStore instance in the kded shell (cleanup)

**What to build:** `ActivityDefaultsSwitcherModule` (`src/module.h`, `src/module.cpp`) currently owns a real `ProfileStore m_store` member used only for `load()` — the profiles it feeds the `Switcher` — while the constructor-injected `ProfileStore` (moved into the `Switcher`) serves only `save()`. Production therefore runs **two** store instances against the same `activity-defaults-switcherrc` file. Both are stateless, so behaviour is correct; this is a redundancy/cleanup, not a bug. Unify so one store instance serves both the load and save paths, and the injected store's `load()` is what the module actually reads (it is dead in `tst_module` today).

**Why (from review):** Finding recorded on ticket 08 (non-blocking), explicitly deferred in `3e7ff67`: unifying requires changing the reviewed `Switcher`'s store ownership/interface, and the implementer chose not to risk that churn inside ticket 08's scope. This ticket makes the deferred finding an explicit, tracked item so it is not lost in the closed ticket's notes.

**Blocked by:** 06 (Terminal component + baseline fallback — the `Switcher`), 08 (kded shell tests — the module seam).

**Status:** done

- [x] The module holds exactly one `ProfileStore` instance, used for both `load()` (feeding `refresh()`/`onDefaultsChanged()`) and `save()` (via the `Switcher`), in both the kded constructor and the injected test constructor.
- [x] The injected `ProfileStore::load()` is exercised in `tst_module` (the fake store's load path is the one the module reads from), and the by-value `m_store` member is gone.
- [x] No behaviour change: `ctest` passes all suites (resolver, writer, switcher, store, module) and the shell suite still never touches the live filesystem/D-Bus.

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- `.scratch/activity-defaults-switcher/issues/08-kded-shell-tests.md` — "Notes from review of 08" (the deferred finding).
- `src/module.h`, `src/module.cpp` — `m_store` member and `refresh()`/`onDefaultsChanged()`.
- `src/switcher.h`, `src/switcher.cpp` — the `Switcher`'s `std::unique_ptr<ProfileStore>` ownership.
- `tests/tst_module.cpp` — `FakeStore` (its `load()`/`loads` are currently unused).
- `CONTEXT.md` — vocabulary.
