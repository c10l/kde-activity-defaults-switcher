# 18: Fix `.clang-format` to match the committed style + missing `override`

**What to build:** Two tooling/hygiene fixes surfaced during the ticket-09 review, both independent of that ticket's logic:

1. **`.clang-format` contradicts the committed house style**, so `make format-check` fails on *every* file with clang-format 21 (and would with any modern clang-format). The config claims to match "the existing hand-written style" but three knobs are wrong:
   - `PointerAlignment: Left` emits `Type* ptr`, but the codebase writes `Type *ptr` (and `const QString &id`) — i.e. `PointerAlignment: Right`.
   - `AccessModifierOffset: -2` (an LLVM default) indents `public:`/`private:` to `  public:`, but the codebase leaves them at column 0 → `AccessModifierOffset: -4`.
   - `BreakBeforeBraces: Linux` forces the namespace opening brace onto a new line (`namespace ads\n{`), but the codebase keeps `namespace ads {` on one line → switch to `BreakBeforeBraces: Custom` with `BraceWrapping.AfterNamespace: false` (and re-enable `AfterClass`/`AfterFunction`/`AfterStruct`/`AfterControlStatement: Never` to keep the rest of the `Linux` look).
   - `AlignTrailingComments` (default `true`) aligns trailing comments across adjacent members, but the codebase writes a single space before each (`m_editTimer; // ...`, `m_store; // ...`) → `AlignTrailingComments: false`.

   Even after those fixes, two committed lines are hand-formatted in a way no config reproduces and would still trip `--Werror`: the manual wrap of the second `ActivityDefaultsSwitcherModule` constructor (`QObject *parent,` on its own line in `src/module.h`) and the non-aligned trailing comments on `m_editTimer`/`m_store`. Decide whether to (a) reformat those couple of lines to the config's canonical output, or (b) remove them from the `SOURCES` the format-check covers — but the goal is a `make format-check` that is green on a clean tree with clang-format 21.

2. **`KActivitiesActivityBackend::activityName` is missing `override`** (`src/kactivities_backend.h:31`). Its base `ActivityBackend::activityName` is `virtual ... = 0`, and every other override in that class (`isRunning`, `currentActivityId`, `allActivities`) is marked `override`. clang-tidy flags this as `clang-diagnostic-inconsistent-missing-override`. Trivial one-line fix.

**Why (from review):** Recorded during the ticket-09 review. Ticket 09's implementer correctly scoped its diff to the 5 files it changed and wrote additions to match the surrounding style rather than reformat the whole tree; but the underlying `.clang-format` mismatch is a pre-existing repo bug that will bite every future ticket's `make check`, and the missing `override` is the only real clang-tidy finding in the codebase (now that clang-tidy is installed). Both are tracked here rather than left as notes in the closed ticket.

**Blocked by:** none.

**Status:** done

- [x] `.clang-format` sets `PointerAlignment: Right`, `AccessModifierOffset: -4`, `AlignTrailingComments: false`, and `BreakBeforeBraces: Custom` with `BraceWrapping.AfterNamespace: false` (keeping `AfterClass`/`AfterFunction` true and `AfterControlStatement: Never`; `AfterStruct: false` — see review note), and the header comment is updated to describe the real style.
- [x] `make format-check` passes on a clean tree with clang-format 21 (any residual hand-formatted lines in `src/`/`tests/` are either brought in line with the config or removed from the format target).
- [x] `KActivitiesActivityBackend::activityName` is marked `override`.
- [x] `make tidy` reports no real warnings (only the pre-existing suppressed/3rd-party noise), and `ctest` stays green.

### Notes from review of 18

- The ticket's `AfterStruct: true` was a factual error: the committed style writes `struct Activity {` and `struct Binding {` with the brace on the same line (`src/domain.h`, unchanged before and after this ticket). `AfterStruct: true` would have moved those braces and failed criterion 2, contradicting the ticket's own "match the committed style" premise. The implementation's `AfterStruct: false` is correct and required; criterion 1 corrected accordingly.

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- `.clang-format` — current config and its mis-set knobs.
- `Makefile` — `format`/`format-check`/`tidy`/`lint`/`check` targets and `SOURCES`.
- `src/kactivities_backend.h` — the missing `override` on `activityName`.
- `src/activity_backend.h` — the `virtual ... = 0` base decl.
- `deps/usr/include/PlasmaActivities/plasmaactivities/info.h`, `consumer.h` — the KActivities semantics (unchanged).
