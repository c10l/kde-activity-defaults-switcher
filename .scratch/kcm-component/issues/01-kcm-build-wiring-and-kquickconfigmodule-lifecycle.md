# 01: KCM build wiring and the KQuickConfigModule lifecycle

Type: research
Status: resolved

## Question

What are the exact build-time and runtime requirements to add a QML `KQuickConfigModule` to this project, and what does KQuickConfigModule's load/save/apply/defaults lifecycle look like?

- Which `find_package` components and CMake targets are needed (`KF6::KCMUtils`, KDeclarative/KQuickAddons, `Qt6::Qml`/`Quick`), and how is the module built and installed to `${KDE_INSTALL_KCMDIR}` so System Settings discovers it?
- The KPlugin/KCM JSON metadata (`X-KDE-ServiceTypes`, parent app, System Settings category/keywords) and the CMake mechanism that registers it (`K_PLUGIN_CLASS_WITH_JSON`, `kcoreaddons_add_plugin`, `ecm_add_qml_module`, …).
- How a `KQuickConfigModule` is authored: the C++ subclass constructor signature, the QML view pairing, and the lifecycle virtuals — `load()`, `save()`, `defaults()`, the changed/needsSave state, and the Apply/Defaults buttons.
- Which dev packages to add to `scripts/setup-deps.sh` so `make ci` / CI install them.

Primary sources: the KCMUtils/KDeclarative dev headers, ECM/KDEInstallDirs docs, and the kcmutils API docs.

## Answer

Sources: KCMUtils/KDeclarative 6.28.0 dev headers (extracted from `libkf6kcmutils-dev`/`libkf6declarative-dev`), `KF6KCMUtilsMacros.cmake`, `KDEInstallDirs6.cmake`.

**Build deps (add to `scripts/setup-deps.sh`):** `libkf6kcmutils-dev` (itself Depends on `libkf6configwidgets-dev`, `qt6-declarative-dev`, `qml6-module-org-kde-kcmutils`, `libkf6coreaddons-dev`, `qt6-base-dev`), plus `libkf6declarative-dev`, and `kirigami2-dev` + `qml6-module-org-kde-kirigami` for the QML UI (the KCM shell uses Kirigami units/PageRow).

**CMake targets & helper:** `find_package(KF6KCMUtils)` provides imported targets `KF6::KCMUtils`, `KF6::KCMUtilsQuick`, `KF6::KCMUtilsCore`; `find_package(KF6Declarative)` provides `KF6::Declarative`. The KF6 helper is `kcmutils_add_qml_kcm(<target> SOURCES … INSTALL_NAMESPACE …)` (KF6KCMUtilsMacros.cmake): it wraps `kcoreaddons_add_plugin` (installs to `${KDE_INSTALL_PLUGINDIR}/<namespace>`, default `plasma/kcms/systemsettings`), generates a `.desktop` (`kcmutils_generate_desktop_file`) from a same-basename `.json`, and auto-bundles the `ui/` subdirectory into a `.qrc` under `/kcm/<target>`.

**Correction to ticket 07's assumption:** KF6 ECM (`KDEInstallDirs6.cmake`) defines `KDE_INSTALL_PLUGINDIR`, not `KDE_INSTALL_KCMDIR`; KCMs install as plugins under the `plasma/kcms` namespace, not to a separate `KCMDIR`. The `KPackage`/`kpackage_install_package` text in `kquickconfigmodule.h`'s doxygen is stale legacy; the qrc-based `kcmutils_add_qml_kcm` is the current mechanism.

**Authoring:** subclass `KQuickConfigModule`; register with `K_PLUGIN_CLASS_WITH_JSON(MyKCM, "kcm.json")`; constructor `MyKCM(QObject *parent, const KPluginMetaData &metaData)`. QML lives in `ui/main.qml`, and reaches the module via the global `kcm` object or the `org.kde.kcmutils` `ConfigModule` attached property.

**Lifecycle (`KAbstractConfigModule`):** virtuals `load()` (called after construction and on Reset), `save()` (on Apply/OK), `defaults()` (on the Default/"Use Defaults" button). Properties: `needsSave` (`setNeedsSave(true)` when the user changes something), `representsDefaults`, `supportsInstantApply` (default **true** — set `false` for Apply-button semantics), and `buttons` (`NoAdditionalButton=0, Help=1, Default=2, Apply=4, Export=8`; without `Apply` the shell shows "Close"). The Default button drives `defaults()`, not `save()`.
