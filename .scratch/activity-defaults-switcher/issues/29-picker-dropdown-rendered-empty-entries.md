# 29: The picker dropdown rendered one narrow strip of empty entries

**What to fix:** Opening any picker in the KCM showed a broken dropdown — a
~30px vertical strip of clipped, nameless entries overlapping the rows below it
(screenshot from the user's session). Measured offscreen: the ComboBox was
324px wide, its popup 28px, and every entry's `text` was empty.

Cause (measured, not guessed): the delegate in `kcm/ui/ComponentComboBox.qml`
was written for Qt's Basic style, which hosts a ComboBox's entries in the
control's own `ListView`:

```qml
delegate: QQC2.ItemDelegate {
    width: ListView.view.width      // null in the KDE styles' Menu popup
    text: model.displayName        // the context `model` is not the entry there
    icon.name: model.icon
}
```

The KDE styles — `org.kde.desktop`, which System Settings uses, and
`org.kde.breeze` — host the entries in a **Menu popup** instead: the desktop
style has `popup: Menu { implicitWidth: <max entry implicitWidth>; Repeater {
model: controlRoot.delegateModel } }`, and Breeze has `popup: Menu { ... delegate:
control.delegate }`. In that shape `ListView.view` is null, so `width` threw
`TypeError: Cannot read property 'width' of null` and the entry's model never
resolved, leaving `text` empty and `implicitWidth` at the icon's 20px — which is
what the desktop style sizes the popup from (20 + padding = 28).

Fix, following what the styles' own delegates do:

- Bind the entry as `required property var model` and read
  `model[comboBox.textRole]` (exactly the desktop and Breeze default delegates),
  not the `model` context property.
- Size from the parent the style gives the entry — `width: parent ? parent.width
  : implicitWidth`, Breeze's own delegate idiom, which also holds in Qt's Basic
  style where that parent is the popup's ListView, already as wide as the
  control.
- Use `QQC2.MenuItem`, which is what both KDE styles use for their entries (and
  which the Basic style's ListView hosts fine).

**Why:** The picker is the whole point of the page; a dropdown that shows an
empty strip is unusable. It shipped because no test ever *opened* a popup — the
suite pinned the closed field (`currentText`/`currentValue`) and the rows, and
the desktop style's popup errors were never evaluated.

**Blocked by:** none.

**Status:** done

- [x] An open dropdown renders every entry with its text and icon at the popup's
      width, in all three styles (`org.kde.desktop`, `org.kde.breeze`, Basic).
- [x] The picker's delegate emits no QML warnings when the popup is populated
      (the desktop style instantiates the entries eagerly, so the suite's
      load-time warning check covers it too).
- [x] The offscreen smoke suite runs under `org.kde.desktop` when that style is
      installed — the popup shape System Settings actually uses — falling back to
      the default style where it is not, and passes under all three styles
      (`QT_QUICK_CONTROLS_STYLE` still wins when set explicitly).
- [x] `pickerDropdownListsTheApplications` pins the open dropdown: entry texts,
      and every entry at least as wide as its implicit width and wider than
      100px. Red on the pre-fix delegate (`text` `""` against `"Inherit"`), green
      after.
- [x] `ctest` 8/8, format-check clean, and the production plugin staged with
      `DESTDIR`/`QT_PLUGIN_PATH` still loads in `kcmshell6` with the real
      backends.

### Notes from review of 29

- Fixed a second, test-side defect the desktop style exposed: every ComboBox
  popup in that style contains its own `Repeater` over the delegate model, and
  those items have no `component` property — so the suite's row lookup read
  `property("component").toInt()` as `0` and handed back a *popup* entry as the
  Browser row. `rowForComponent`/`componentRows` now require the property to
  exist (`isFormRow`). This is why the suite now runs under the desktop style:
  the old default-style run could not see either defect.
- Popup width is the style's decision, not ours: the desktop style sizes it to
  the widest entry (152px for short labels), Breeze forces
  `Math.max(control.width, implicitWidth)` (324px). Unchanged, and matching every
  other ComboBox on the platform.
- Verified beyond the assertions: the popup geometry was dumped for all three
  styles (entries 144px wide with the right texts, no TypeErrors), the open
  popup's own window was rendered to a PNG and inspected (Inherit (baseline:
  KMail) / Gwenview / Spectacle, each with its icon, current entry highlighted),
  and the staged production plugin was run through `kcmshell6`.

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- `kcm/ui/ComponentComboBox.qml` — the delegate and its sizing.
- `tests/kcm_qml/tst_kcm_qml.cpp` — `popupEntries`, `isFormRow`,
  `pickerDropdownListsTheApplications`, `initMain`'s style preference.
- Installed styles: `org/kde/desktop/ComboBox.qml` (delegate + `popup: Menu`),
  `org/kde/breeze/ComboBox.qml` (delegate + `popup: Menu { delegate: control.delegate }`),
  `QtQuick/Controls/Basic/ComboBox.qml` (`width: ListView.view.width`).
- Upstream `plasma-workspace/kcms/componentchooser/ui/ComponentComboBox.qml` —
  same delegate shape as the old code, i.e. the same latent bug.
