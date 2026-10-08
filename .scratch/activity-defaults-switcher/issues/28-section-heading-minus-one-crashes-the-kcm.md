# 28: The KCM crashed — a section heading's `-1` reached the domain

**What to fix:** Opening the KCM crashed System Settings (SIGSEGV, coredump
`systemsettings` at 2026-10-09 01:32, and reproducible headlessly: `kcmshell6
activity-defaults-switcher-kcm` exits 139). The stack is

```
QMimeDatabase::mimeTypeForName        (null deref in compareStrings)
KApplicationTrader::queryByMimeType   (libKF6Service)
ads::ApplicationTraderBackend::applicationsFor
ads::KcmBackend::pickerEntries
QML binding in the Repeater delegate during mainUi()
```

Ticket 27's form emits a **section-heading row per section with
`component = -1`**, and every row's picker evaluates
`kcm.backend.pickerEntries(formRow.component)` — including the headings. The
guard in `pickerEntries` was `static_cast<Component>(component) <
Component::Count`, which is **true for every negative value**, so `-1` was
converted to a `Component` and travelled into `primaryMimeTypeFor()`. That
function's switch covers only the enum, so it fell through to
`Q_UNREACHABLE()` — undefined behaviour in a release build — and the caller
received an uninitialized QString. That garbage mime name was handed to
`KApplicationTrader::queryByMimeType`, which dereferenced a null string inside
`QMimeDatabase::mimeTypeForName`.

Fix, in three parts (all three are needed — the boundary must hold even if a
second caller appears):

1. `isValidComponent(int)` in `src/domain.h` — `0 <= value < Count` — and use it
   at every int→`Component` boundary in `KcmBackend` (`pickerEntries`,
   `setBinding`, `clearBinding`). `setBinding`/`clearBinding` had the same
   upper-bound-only check, so a later `-1` edit would have reached the profile
   map and the same switches.
2. `mimeKeysFor`/`primaryMimeTypeFor` no longer end in `Q_UNREACHABLE()`: a value
   that is not a Component yields an empty list / empty string. The switch still
   handles every enumerator (including `Count`), so ticket 19's `-Werror=switch`
   exhaustiveness is unchanged — but the impossible-at-compile-time case is now
   *defined* at run time, because a plain int can reach it.
3. The view does not ask at all: `main.qml`'s picker model binding returns `[]`
   for a section-heading row.

**Why:** The crash is total — System Settings dies on opening the KCM — and it
was masked in the test suite because the smoke test's fake `ApplicationBackend`
answered any `Component` without touching mime types. That is the lesson worth
recording: the view's contract ("only real Components are asked about") needed a
test, not just the fakes' tolerance.

**Blocked by:** none.

**Status:** done

- [x] `pickerEntries`, `setBinding` and `clearBinding` reject a value that is not
      a real Component (negative, `Count`, or past it) instead of converting it.
- [x] `mimeKeysFor`/`primaryMimeTypeFor` are defined (empty) for a non-Component
      rather than `Q_UNREACHABLE()`, and still cover every enumerator.
- [x] The view never asks for a section-heading row's picker entries.
- [x] Regression tests, all three red before the fix and green after:
      `WriterTest::nonComponentHasNoMimeKeys` (**SIGSEGV** on the old code),
      `KcmTest::invalidComponentIsRejected` (`pickerEntries(-1)` returned the
      Inherit entry), `KcmQmlSmokeTest::pickersAreOnlyAskedForRealComponents`
      (the view asked about component -1).
- [x] End-to-end: the fixed plugin staged into a search path
      (`DESTDIR` + `QT_PLUGIN_PATH`) loads in `kcmshell6` and stays up
      (rc 124/`timeout`), where the same command against the installed pre-fix
      plugin exits 139; the running process maps the staged `.so`.

### Notes from review of 28

- Verified red-then-green per test, not just the suite: `nonComponentHasNoMimeKeys`
  segfaults on the pre-fix build, and the two boundary tests fail with the
  messages quoted above. `ctest` 8/8 and format-check clean after the fix.
- Diagnosis came from the crash itself: a coredump backtrace put the fault in
  `KApplicationTrader::queryByMimeType`, and a temporary trace in
  `applicationsFor` printed `component=-1 mime=<garbage>` — the uninitialized
  QString, not a lookup failure. Worth recording that the earlier SIGSEGV seen
  while building ticket 27's render check was **this same crash**, and that it
  was wrongly dismissed then as an artefact of the throwaway harness: the
  standalone probe of `KApplicationTrader::queryByMimeType` passed because it
  passed a *valid* mime name. A reproduction that is dismissed must be explained,
  not just out-of-the-way — the harness was the only place a real enumeration
  backend met the view.
- `kcm_componentchooser` (upstream, and `kcm_componentchooser.so` here) was
  unaffected, which is consistent: it has no section rows and passes enum values,
  not QML ints, into its model.

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- Coredump: `coredumpctl debug 239140` (`/usr/bin/systemsettings`, SIGSEGV) —
  frames `QMimeDatabase::mimeTypeForName` ← `KApplicationTrader::queryByMimeType`
  ← `ads::ApplicationTraderBackend::applicationsFor` ← `ads::KcmBackend::pickerEntries`.
- `src/domain.h` — `isValidComponent`.
- `src/kcm_backend.cpp` — the three boundaries.
- `src/writer.cpp` — `Q_UNREACHABLE()` removed from `mimeKeysFor`/`primaryMimeTypeFor`.
- `kcm/ui/main.qml` — the picker's model binding guard.
- `tests/tst_writer.cpp`, `tests/tst_kcm.cpp`, `tests/kcm_qml/tst_kcm_qml.cpp` — the three tests.
