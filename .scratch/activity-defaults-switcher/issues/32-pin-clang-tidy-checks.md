# 32: Pin the clang-tidy check set so `make tidy` runs on clang-tidy 23

**What to build:** The repo carried no `.clang-tidy`, so `make tidy` ran
clang-tidy with an empty check set. clang-tidy 23 (Homebrew LLVM 23.1.3) made
that fatal: it exits 1 with `Error: no checks enabled.` before analysing
anything — on every file, including untouched ones. Pin an explicit `Checks` set
in a new `.clang-tidy` at the repo root so the gate runs, and runs identically,
on the clang-tidy versions CI (Debian) and contributors (Homebrew LLVM 23) use.

**Why:** Flagged in ticket 18 (its fourth acceptance criterion, which held only
for the clang-tidy current when it was written) and in ticket 27's tooling-drift
note, both of which recorded the failure but deferred *which checks to adopt* as
a project policy call. The owner resolved that call directly, so this ticket was
worked outside the ticket queue (no separate review).

**Blocked by:** none.

**Status:** done

- [x] `.clang-tidy` exists at the repo root and pins `Checks` explicitly:
      `clang-diagnostic-*,clang-analyzer-*,-clang-analyzer-optin*` — compiler
      diagnostics plus the core static analyzer, which is what `make tidy` was
      effectively checking before; the analyzer's opt-in checks are excluded.
- [x] `make tidy` runs to completion on clang-tidy 23.1.3: all 39 sources
      processed, exit 0, no diagnostics on a clean tree.
- [x] README documents the pinned check set and why it is pinned.

**Notes:**

- The opt-in exclusion is not cosmetic:
  `clang-analyzer-optin.core.EnumCastOutOfRange` fires on the deliberate
  out-of-range `Component` casts in `tests/tst_writer.cpp`'s
  `nonComponentHasNoMimeKeys` robustness loop — exactly the "a plain int
  (`-1` from a section heading) must not be UB" case that loop asserts. Those
  checks are meant to be opted into, not enabled by the `clang-analyzer-*`
  wildcard.
- Verified with the failing command itself: `make tidy` exit 0, zero
  `warning:`/`error:` lines, `[39/39] Processing file …`; `clang-tidy -p build
  --explain-config` confirms the checks come from `.clang-tidy`;
  `make format-check` stays clean.
- Because the set is written in the config file (not as a `--checks` argument on
  the `tidy` target), editors and `clangd` see the same set, and no
  version-dependent flag (`--allow-no-checks` does not exist in older
  clang-tidy) is introduced.

**References:**
- `.clang-tidy` — the pinned `Checks` and its rationale.
- `Makefile` — the `tidy`/`lint` targets that were failing.
- `README.md` — development-tasks section.
- `tests/tst_writer.cpp:440` — the intentional out-of-range cast.
- `18-clang-format-config-mismatch-and-missing-override.md`,
  `27-kcm-matches-default-applications-view.md` — where the drift was recorded.
