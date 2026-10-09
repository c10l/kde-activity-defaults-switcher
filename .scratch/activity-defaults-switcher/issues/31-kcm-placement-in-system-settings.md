# 31: System Settings placement — the KCM now sits in the Default Applications group

**The question:** can the per-activity page be made a *child* of the upstream
"Default Applications" settings page?

**The answer: no, not as a page inside it.** What System Settings offers is:

1. **Category placement**, metadata only:
   `X-KDE-System-Settings-Parent-Category` "defines the category systemsettings is
   showing the module in" ([KCM development docs](https://develop.kde.org/docs/features/configuration/kcm/)).
   It groups a module with others that declare the same category id — it cannot
   make one KCM live inside another.
2. **Sub-pages belong to the KCM's own package**: `kcm.push("AnotherPage.qml")`
   (same docs), backed by `KQuickConfigModule::push/subPage/depth/currentIndex`
   (`KQuickConfigModule`, KCMUtilsQuick 6.30) — the pages are files in *that*
   KCM's QML resource, rendered as a Kirigami PageRow. There is no metadata key
   and no API for a module to attach itself to another module's page stack.
3. Upstream's `kcm_componentchooser` is a compiled plugin shipped by
   plasma-workspace with a fixed `ui/main.qml` (the FormLayout read while
   building ticket 27) and no extension point, so nothing can be injected into
   its page. A true child page would take an upstream change — an MR adding a
   page that our module provides — which upstream has no reason to carry for a
   third-party module.

**What was done instead (the closest the framework allows):** our KCM now
declares the **same category as Default Applications**, so the two sit in the
same System Settings group. Measured on this system:

| module | parent category | weight |
| --- | --- | --- |
| `kcm_componentchooser` (Default Applications) | `applications-defaults` | 10 |
| `kcm_filetypes` (File Associations) | `applications-defaults` | 20 |
| `activity-defaults-switcher-kcm` (was `applications`) | `applications-defaults` | 30 |

So the per-activity page appears in that group directly after Default
Applications and File Associations, instead of as an entry of its own under
`applications` (where it sat beside Activities and Notifications).

**Also available, not done:** a one-way link *from* our page *to* theirs,
`KCM.KCMLauncher.openSystemSettings("kcm_componentchooser")` — the idiom upstream
itself uses for related settings (its `ComponentOverlay` opens `kcm_filetypes`).
The reverse direction is the impossible one.

**Why:** the user asked for the per-activity page to be associated with the
Default Applications page rather than floating on its own; category placement is
the only mechanism the platform provides, and it is one line of metadata.

**Blocked by:** none.

**Status:** done

- [x] `kcm/activity-defaults-switcher-kcm.json` declares
      `X-KDE-System-Settings-Parent-Category: applications-defaults` and
      `X-KDE-Weight: 30`, matching the form the installed KCMs use (a bare JSON
      number for the weight).
- [x] The placement and loadability are pinned:
      `KcmQmlSmokeTest::kcmMetadataLoadsAndPlacesItWithDefaultApplications` reads
      the built production plugin's metadata and asserts it is valid, carries the
      module id, and names that category. Red (`Actual (""): Expected
      ("applications-defaults")`) with the key removed; green with it.
- [x] `ctest` 8/8, format-check clean, and the staged production plugin is the
      one `kcmshell6` loads (checked in `/proc/<pid>/maps`) and stays up.

### Notes from review of 31

- **A false lead worth recording.** While verifying, a probe reported the
  plugin's metadata as invalid (`valid: false`, no id) for *both* the old and new
  json, which read as "the metadata is broken". The probe passed a **relative**
  path: `QPluginLoader` reports `The shared library was not found.` for it and
  `KPluginMetaData` then comes back empty for a perfectly good plugin. With an
  absolute path the same file is `valid: true` with every key. The
  `X-KDE-Weight` value type was briefly suspected (number vs string) and is *not*
  a problem: installed KCMs store it as a JSON number (`type=2`, e.g.
  `kcm_activities` 40, `kcm_componentchooser` 10) and ours matches. The test
  carries a comment about the absolute-path requirement so the next person does
  not repeat the detour.
- Not verified visually: System Settings' sidebar was not rendered (its windows
  are offscreen; the VNC platform needs a client this box does not have), so the
  grouping is established from the mechanism and the shared category id rather
  than from a screenshot. One glance at System Settings confirms it; the change
  is a one-line revert if the page is wanted back under `applications`.
- The ticket-30 claim was re-checked here with `/proc/<pid>/maps`: the staged
  plugin is genuinely the loaded one (ticket 29's check, which ticket 30's
  earlier run had skipped).

**Suggested skills:** `review-ticket` (reviewer).

**References:**
- `kcm/activity-defaults-switcher-kcm.json` — the placement.
- `tests/kcm_qml/tst_kcm_qml.cpp` — `kcmMetadataLoadsAndPlacesItWithDefaultApplications`.
- `CMakeLists.txt` — the `ADS_KCM_PLUGIN` definition the test reads.
- `/usr/include/KF6/KCMUtilsQuick/kquickconfigmodule.h` — `push`, `subPage`, `depth`.
- KDE docs: [KCM development](https://develop.kde.org/docs/features/configuration/kcm/).
- Installed evidence: `X-KDE-System-Settings-Parent-Category` / `X-KDE-Weight` in
  `kcm_componentchooser.so`, `kcm_filetypes.so`, `kcm_activities.so`.
