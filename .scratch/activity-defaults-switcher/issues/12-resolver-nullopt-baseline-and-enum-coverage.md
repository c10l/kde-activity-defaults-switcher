# 12: Resolver nullopt-baseline path + enum-coverage guard

**What to build:** Two domain-layer test gaps:

1. `Resolver::resolve` with `baselineActivity == std::nullopt` is never exercised — every resolver test passes a concrete baseline `Activity`, so the branch that treats the baseline profile as empty is untested.
2. There is no guard that every `Component` enum value is covered by `allComponents()`, `componentKey()`, `mimeKeysFor()`, and `primaryMimeTypeFor()`. A new enum value omitted from these switch statements falls through to the empty-return default silently, which would corrupt the profile store or silently skip a component.

**Blocked by:** 01 (Resolver + domain model).

**Status:** done

- [x] A resolver test covers `resolve` with `std::nullopt` baseline: the current activity's binding still applies, the baseline is treated as empty, and the current default is captured when no binding exists.
- [x] A test (or compile-time guard) asserts every `Component` enum value appears in `allComponents()` and has a non-empty `componentKey()`.
- [x] The enum-coverage guard also covers `mimeKeysFor()`/`primaryMimeTypeFor()` for non-terminal components, while the intentional empty terminal keys remain allowed.

**Note from review of 12:** the runtime guard pins today's 13 members; a brand-new member never referenced anywhere stays invisible to runtime tests. Compile-time hardening tracked in ticket 19.

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- `src/domain.cpp` — `Resolver::resolve`, `allComponents`, `componentKey`.
- `src/writer.cpp` — `mimeKeysFor`, `primaryMimeTypeFor`.
- `tests/tst_resolver.cpp` — existing resolver coverage.
- `CONTEXT.md` — vocabulary.
