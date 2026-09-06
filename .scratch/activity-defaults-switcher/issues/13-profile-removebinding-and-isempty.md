# 13: Remove or test the unused Profile methods

**What to build:** `Profile::removeBinding` and `Profile::isEmpty` (`src/domain.h`, `src/domain.cpp`) are declared and defined but never used or tested. Decide: either add tests that exercise them through the public interface, or remove them (noting they can return with the ticket-07 KCM, which is where reset/removal is scoped).

**Blocked by:** 01 (Resolver + domain model).

**Status:** ready-for-agent

- [ ] `removeBinding` and `isEmpty` are either tested or removed (do one, not both).
- [ ] If kept, a test pins `removeBinding` (a removed component no longer has a binding) and `isEmpty` (empty vs non-empty profile).

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- `src/domain.h`, `src/domain.cpp` — `Profile`.
- `tests/tst_resolver.cpp` — `profileExposesBindings` (the existing Profile test).
- `.scratch/activity-defaults-switcher/issues/07-kcm-component.md` — where reset/removal belongs.
- `CONTEXT.md` — vocabulary.
