# 27: KCM view mirrors the System Settings Default Applications page

**What to build:** Rebuild the KCM's view so it reads like System Settings' Default
Applications page (upstream `componentchooser`) and its pickers behave the same way.

- Group the Components under that page's four section headings — Internet,
  Multimedia, Documents, Utilities — in that page's reading order, in a
  `Kirigami.FormLayout` with a section-header row (`Kirigami.FormData.isSection`)
  per heading and that page's per-Component labels ("Web browser:", "Email
  client:", "Terminal emulator:", …) as `Kirigami.FormData.label`, right-aligned.
- One picker per Component, in a new `kcm/ui/ComponentComboBox.qml` mirroring
  upstream's: a `QQC2.ComboBox` whose closed field carries the Application's icon
  (`Kirigami.StyleHints.iconName`, which the org.kde.desktop style draws) and
  whose delegate shows icon + name.
- Plumb the icons from the backend: `ApplicationCandidate` gains `iconName`
  (`KService::icon()`, generic icon when the entry names none),
  `ApplicationBackend` gains `iconNameFor()` for a binding the enumeration does
  not list, and `KcmBackend::pickerEntries` emits an `icon` per entry (the undo
  icon for the Inherit entry, which is not an Application).
- `ComponentRowsModel` carries the grouping: section-header rows plus the
  Component rows in display order, exposing `section`/`isSection` roles. The
  form's structure is static, so `setBindings` now updates the rows in place
  instead of resetting the model.
- The picker's selection is re-derived from both its inputs (the row's effective
  Application and its entry list) whenever either changes.

**Why:** The first view was a bare two-column list — a plain label per Component
and a text-only combo, no grouping, no icons — and the user asked for it to look
like, and its pickers to behave like, the Default Applications page.

Upstream's chooser is **not reusable**: `ComponentComboBox.qml`,
`ApplicationModel`, `ComponentChooser`, `ComponentOverlay` and `KcmPopupModal`
are private to `kcm_componentchooser` (plasma-workspace `kcms/componentchooser`),
compiled into that KCM's plugin and its `ui/` qrc, GPL-2.0-or-later, and not
installed for other modules to import or link. What *is* upstream and already a
dependency here is the idiom this ticket reuses: `Kirigami.FormLayout` /
`Kirigami.FormData`, `Kirigami.StyleHints.iconName`, `QQC2.ComboBox` with an
`icon.name` delegate, and `KApplicationTrader` / `KService::icon()` (already the
enumeration source in `ApplicationTraderBackend`).

**Blocked by:** none.

**Status:** done

- [x] The view is a `Kirigami.FormLayout` with the four Default Applications
      section headings, that page's per-Component labels, and right-aligned
      labels; every Component appears exactly once, in that page's order.
- [x] One picker per Component shows the Application's icon beside its name in
      the closed field and in every entry; `pickerEntries` carries an `icon` per
      entry (an Application's own icon, the generic icon for a binding the
      enumeration does not list, the undo icon for the Inherit entry).
- [x] Every picker holds the Component's Activity semantics: the Inherit entry
      first, the baseline fallback rendered in italic, Apply-button saves, and
      per-activity Reset.
- [x] Switching the selected Activity retargets every picker, including when the
      picker's entry list grows in the same update (a binding the enumeration
      does not list).
- [x] `ctest` is green (8/8) and warning-free, including the offscreen QML smoke
      suite, and `make format-check` is clean.

### Notes from review of 27

- **Found while verifying the render: the picker could show the wrong entry.**
  The row's effective Application and the picker's entry list are recomputed in
  the same update but not in a helpful order: the list can arrive *after* the
  Application (a switch onto a profile bound to an Application the enumeration
  does not list appends that entry). The old `syncSelection()` ran on the
  Application change alone, so it scanned the stale list and left the picker on
  the Inherit entry. `ComponentComboBox` now re-derives its selection on the
  entry-list change as well, and no longer relies on an imperative call from
  `main.qml`. Pinned by `switchingActivityRetargetsThePickers` — red on the old
  code (`currentValue` `""` where `org.kde.ark.desktop` was expected), green
  after the fix.
- The model no longer resets: a reset destroys and rebuilds every delegate, and
  Kirigami's `FormLayout` raises `TypeError: Cannot read property 'Accessible'
  of null` / `Unable to assign [undefined] to int` from its own torn-down-row
  paths. Since the form's shape is fixed, `setBindings` updates in place. This
  is what keeps the smoke suite's no-warnings contract satisfiable with a
  `FormLayout`.
- Verified visually, not only by assertion: with `QT_QUICK_CONTROLS_STYLE=org.kde.desktop`
  the view was rendered offscreen to a PNG and inspected — category headings,
  right-aligned labels, and Application icons in the closed fields (KMail,
  KOrganizer, Gwenview, Elisa, Kate, Okular, Konsole, Ark, Marble all resolved
  from the theme). The throwaway render harness was removed afterwards.
- Deliberate non-goals, matching the picker parity that was asked for and the
  KCM design map: upstream's "Other…" free-text entry (`KOpenWithDialog`, which
  the map already puts out of scope for v1), the mime-details overlay
  (`ComponentOverlay`), and `KCM.SettingHighlighter` (the KCM's analogue is the
  italic fallback marker, since "differs from the system default" has no
  per-Activity meaning here).
- **Follow-up from ticket 28 — this ticket's section rows crashed the KCM.** The
  heading rows carry `component = -1`, every row's picker evaluated
  `pickerEntries(formRow.component)`, and that guard accepted the negative value,
  so `-1` reached `primaryMimeTypeFor()`'s `Q_UNREACHABLE()` and the garbage
  QString it returned killed System Settings inside `QMimeDatabase`. Fixed and
  pinned in ticket 28; the smoke suite could not catch it because its fake
  `ApplicationBackend` tolerated any `Component`.
- **Follow-up from ticket 29 — this ticket's picker delegate broke the
  dropdown.** `ComponentComboBox`'s entry delegate was written for Qt's Basic
  style (`width: ListView.view.width`, the `model` context property); the KDE
  styles host the entries in a Menu popup, where `ListView.view` is null, so
  every entry rendered empty and the popup collapsed to a 28px strip. Fixed and
  pinned in ticket 29, which also makes the smoke suite run under
  `org.kde.desktop` so this shape is exercised.
- Tooling drift (not this ticket's scope, recorded in ticket 18): with
  Homebrew LLVM clang-tidy 23, `make tidy` fails on *any* file — including
  untouched ones — with `Error: no checks enabled.`, because the repo carries no
  `.clang-tidy` and that version made the empty-checks case fatal.

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- `kcm/ui/main.qml` — the form: activity header, `Kirigami.FormLayout`, rows.
- `kcm/ui/ComponentComboBox.qml` — the picker (icon field, icon delegate, selection sync).
- `src/component_rows_model.{h,cpp}` — section rows, display order, labels.
- `src/application_backend.h`, `src/application_trader_backend.cpp` — `iconNameFor`.
- `src/kcm_backend.{h,cpp}` — `pickerEntries`' `icon` key.
- `tests/kcm_qml/tst_kcm_qml.cpp` — `formGroupsComponentsLikeTheDefaultApplicationsPage`,
  `switchingActivityRetargetsThePickers`.
- Upstream: `plasma-workspace/kcms/componentchooser/{ui/main.qml,ui/ComponentComboBox.qml,applicationmodel.cpp}`.
