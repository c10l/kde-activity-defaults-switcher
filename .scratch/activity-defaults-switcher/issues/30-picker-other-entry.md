# 30: The picker offers the Default Applications page's "Other…" entry

**What to build:** The last entry of every picker is now "Other…", like the
System Settings Default Applications page's: an escape hatch to an Application
the Component's own enumeration does not offer (one that does not advertise the
Component's mime type, or any other the user wants).

- `KcmBackend::pickerEntries` appends the entry after the enumerated ones:
  `displayName` "Other…", the generic `application-x-shellscript` icon the
  upstream model uses, no `storageId` (it is an action, not an Application), and
  a new `isOtherEntry` flag (`isInheritEntry` false, so the two nameless entries
  stay distinguishable).
- The choice itself comes from `KOpenWithDialog`, through a new
  `ApplicationChooser` interface (`src/application_chooser.h`) the backend takes
  by injection, mirroring `ActivityBackend`/`ApplicationBackend`/`ProfileStore`:
  `KcmBackend::chooseApplication(component)` asks the chooser and binds what it
  returns, buffered like any other edit. A cancelled choice, a chooser that
  returns no Application, an invalid component and the absence of a chooser all
  change nothing.
- The production implementation (`kcm/kopenwithdialog_chooser.cpp`) is
  KOpenWithDialog, built the way upstream's componentchooser builds it (empty URL
  list, the Component's canonical mime type, `setSaveNewApplications(true)`), and
  it lives in the KCM plugin, not in `ads-kcm`: the backend and its tests stay
  free of Widgets and KIO.
- The view routes the entry to the chooser and re-derives the picker's selection
  afterwards either way, so a cancelled chooser cannot leave the field stuck on
  an entry that is only ever an action.

**Why:** The page is meant to read like Default Applications; without the entry
there is no way to bind an Application the enumeration misses (which is exactly
what that entry is for upstream - a binding whose Application is not in the list
could otherwise only be reached by editing the config file).

**Blocked by:** none.

**Status:** done

- [x] The picker's entries end with "Other…" (no storageId, `isOtherEntry`), and
      it is the only entry carrying that flag.
- [x] Choosing it asks the injected `ApplicationChooser` for that Component and
      binds the returned Application (explicit, buffered until Apply); a
      cancelled or empty choice changes neither the rows nor the buffer, and an
      invalid component never reaches the chooser.
- [x] The production chooser is KOpenWithDialog over the Component's canonical
      mime type, in the KCM plugin only; the build still configures, builds and
      passes its tests with KIO absent (the KCM is skipped, `ads-kcm` is not).
- [x] `ctest` 8/8 with the new tests, format-check clean, and the production
      plugin staged with `DESTDIR`/`QT_PLUGIN_PATH` still loads in `kcmshell6`.

### Notes from review of 30

- Verified the production chooser by running it, since fakes cannot cover it: a
  throwaway program (outside the repo, since it drives a foreign modal dialog)
  instantiated `KOpenWithDialogChooser` and called `choose(Component::Browser)`
  offscreen - a real dialog captioned "Choose Application" appeared, and
  rejecting it returned `nullopt`. The concrete adapter is glue over an upstream
  dialog, the same footing as `KActivitiesActivityBackend`, whose contract is
  pinned through the injected fake.
- The pixel check: the picker's real `pickerEntries` rendered dropdown-open shows
  ↺ Inherit / Falkon / Firefox (highlighted) / `>_` Other…, i.e. the same last
  entry, flag and icon as upstream's model. Rendered to a PNG, inspected, then
  the throwaway harness was removed.
- Free text is *not* added: upstream's Other… also lets a bare command be typed,
  and KOpenWithDialog's "save new applications" writes it a desktop entry, so a
  typed command that gets one is bindable here too - but a command with no
  storageId is deliberately ignored (a Binding is a storageId), and the KCM
  design map keeps free-text out of v1.
- Adds `libkf6kio-dev` to `scripts/setup-deps.sh` (shared by CI and `make deps`)
  and to the README's KCM requirements, and guards the KCM build on KF6KIO being
  found - the same pattern as the other optional KCM deps.

**Suggested skills:** `work-ticket`, `tdd` (implementer); `review-ticket` (reviewer).

**References:**
- `src/application_chooser.h`, `src/kcm_backend.{h,cpp}` - the interface, the entry, `chooseApplication`.
- `kcm/kopenwithdialog_chooser.{h,cpp}`, `kcm/kcm.cpp` - the production chooser and its wiring.
- `kcm/ui/main.qml` - the `isOtherEntry` branch.
- `tests/tst_kcm.cpp` (`pickerEndsWithTheOtherEntry`, `otherEntryBindsTheChosenApplication`,
  `otherEntryWithoutAChoiceChangesNothing`), `tests/kcm_qml/tst_kcm_qml.cpp`
  (`otherEntryAsksTheChooserAndBindsTheAnswer`, `cancellingTheOtherEntryKeepsTheRowsApplication`,
  `pickerDropdownListsTheApplications`).
- Upstream: `plasma-workspace/kcms/componentchooser/{applicationmodel.cpp,componentchooser.cpp}` -
  the "Other…" entry and the KOpenWithDialog it opens.
