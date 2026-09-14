# 26: Give the kded module a D-Bus-valid name

**What to build:** The kded6 module's plugin id is `activity-defaults-switcher-kded`, derived from the `.so` basename because `src/module.json` has no `KPlugin.Id`. kded6 uses that id as the module name and builds a D-Bus object path `/modules/<id>` from it, but D-Bus path elements may only contain letters, digits and underscores — hyphens are invalid. On every kded6 start the module therefore logs:

```
QDBusObjectPath: invalid path "/modules/activity-defaults-switcher-kded"
kf.dbusaddons: The kded module name "activity-defaults-switcher-kded" is invalid!
```

The module still autoloads and runs (verified below), but it is never registered as a D-Bus object under `org.kde.kded6`: it cannot be `unloadModule`d or introspected, and it emits a warning at every start. Give the module a valid name.

**Why (from investigation):** Found while diagnosing an unrelated report (the KDE Welcome Center appearing after a kded6 restart). In KF6 `kded.cpp` (v6.28.0), `loadModule()` does `const QString moduleId = module.pluginId();` and then `kdedModule->setModuleName(moduleId)`; `KPluginMetaData::pluginId()` returns the JSON `Id` if present, otherwise the plugin file's basename. With no `Id` in `src/module.json`, the hyphenated `.so` basename becomes the module name and is rejected as a D-Bus path element. KDE's own `wpad-detector` kded module has the same defect, so it is tolerated today — but it is noise, it blocks the module's D-Bus interface, and it is a latent break if a future kded hard-rejects invalid names. The daemon's actual function is unaffected: kded6 holds inotify watches on the four paths `ActivityDefaultsSwitcherModule::init()` arms (`~/.config`, `mimeapps.list`, `kdeglobals`, `activity-defaults-switcherrc`), and `org.kde.kded6.isModuleAutoloaded` reports `true`.

**Verification first (grill the user):** Confirm the fix approach before writing code — either (a) add an `Id` to `src/module.json` (minimal: the `.so` filename stays `activity-defaults-switcher-kded.so`), or (b) rename the CMake target and `.so` to the valid name (conventional: also renames the installed file and the daemon name referenced in ticket 07). Confirm the chosen name (e.g. `activity_defaults_switcher`) and that a differing `Id` still loads cleanly under real kded6 if (a) is chosen.

**Blocked by:** none.

**Status:** ready-for-agent

- [ ] The kded plugin's `KPluginMetaData::pluginId()` is a valid D-Bus path element (matches `[A-Za-z0-9_]+`) and is no longer `activity-defaults-switcher-kded`.
- [ ] A live kded6 start with the module installed produces no `The kded module name ... is invalid!` / `invalid path "/modules/..."` warning for this module (`kquitapp6 kded6`, restart, then `journalctl --user -b | grep -iE 'invalid path|module name .*is invalid'` shows nothing for it).
- [ ] The module still autoloads and works: `org.kde.kded6.isModuleAutoloaded <id>` is `true`, it appears in `loadedModules`, and the ticket-04 end-to-end check still holds (switch activity → the current activity's binding is applied).
- [ ] A regression test pins the valid plugin id — e.g. a test that loads the built plugin's `KPluginMetaData` (via a `$<TARGET_FILE:...>` compile definition, guarded like the KCM smoke plugin when the kded deps are present) and asserts the id is D-Bus-valid — verified red before the fix.

**Suggested skills:** `work-ticket` (implementer — verify and grill the user on approach); `review-ticket` (reviewer).

**References:**
- `src/module.json` — the `KPlugin` block missing `Id`.
- `CMakeLists.txt` — `add_library(activity-defaults-switcher-kded MODULE ...)`, its `install(TARGETS ...)` and the paired `install(FILES src/module.json ...)`.
- `src/module.cpp` / `src/module.h` — `ActivityDefaultsSwitcherModule`, `K_PLUGIN_CLASS_WITH_JSON(..., "module.json")`.
- kded6 v6.28.0 source: `src/kded.cpp` — `loadModule()`, `moduleId = module.pluginId()`, `setModuleName(moduleId)`.
- `tests/tst_module.cpp` and ticket 08 — the injected-fakes seam for the module shell.
- `.scratch/kcm-component/issues/07-i18n.md` — the note pinning the daemon name as `activity-defaults-switcher-kded` (parallel to the KCM's `activity-defaults-switcher-kcm`).
