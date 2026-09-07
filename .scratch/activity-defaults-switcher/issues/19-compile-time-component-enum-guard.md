# 19: Compile-time guard for Component enum coverage

**What to build:** Ticket 12 pinned, at runtime, that the current 13 `Component` members appear in `allComponents()` and have non-empty `componentKey()` / `mimeKeysFor()` / `primaryMimeTypeFor()`. A runtime test cannot see a brand-new enum member that nothing references yet: add a `Component` value to the enum and forget it in `allComponents()`, and every test still passes while the component is silently never resolved or written — the exact silent-skip failure ticket 12 exists to prevent. Make the guard compile-time so an omitted case fails the build: exhaustive switches (no trailing empty-return default, relying on `-Wswitch` and `-Werror` to reject a missing case) for `componentKey()`, `mimeKeysFor()`, `primaryMimeTypeFor()`, and a sentinel/`static_assert` (or equivalent) so `allComponents()` cannot drift from the enum's member count.

**Blocked by:** 03 (Writer) — spans `src/domain.cpp` and `src/writer.cpp`.

**Status:** done

- [x] Adding a `Component` enumerator to `src/domain.h` and omitting it from `allComponents()`, `componentKey()`, `mimeKeysFor()`, or `primaryMimeTypeFor()` fails the build rather than silently falling through to the empty return.
- [x] The terminal's intentional empty mime keys and primary mime type still compile and behave as today (kdeglobals, ADR-0001).
- [x] Existing resolver and writer tests still pass; `make format-check` clean.

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- `src/domain.cpp` — `allComponents`, `componentKey`.
- `src/writer.cpp` — `mimeKeysFor`, `primaryMimeTypeFor`.
- `tests/tst_resolver.cpp`, `tests/tst_writer.cpp` — ticket 12's runtime guards.
- Ticket 12 review note: the runtime pin covers today's member set; only a compile-time guard catches a not-yet-referenced new member.
