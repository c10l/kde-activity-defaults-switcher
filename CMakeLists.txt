# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 c10l

cmake_minimum_required(VERSION 3.16)
project(activity-defaults-switcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
# The KCM plugin is a loadable MODULE. kcoreaddons_add_plugin (used by
# kcmutils_add_qml_kcm) builds a STATIC plugin unless BUILD_SHARED_LIBS is ON;
# the project's own libraries are declared STATIC explicitly, so this only
# affects the plugin.
set(BUILD_SHARED_LIBS ON)
# The static libraries are linked into the kded6 shared module, so they must
# be position-independent.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# KDE install locations (${KDE_INSTALL_PLUGINDIR}, ${KDE_INSTALL_KCMDIR}, …)
# used by the install() rules below. Included before find_package(Qt6) so
# KDEInstallDirs — not Qt's GNUInstallDirs-equivalent — owns CMAKE_INSTALL_*.
# 5.240 is the ECM release that defaults to Qt6 when Qt6 isn't found yet.
find_package(ECM 5.240 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

# kded6 discovers modules under Qt's plugin directory, which lives in /usr on
# system installs. Default the prefix there (instead of CMake's /usr/local) so
# a plain `make install` lands the module where kded6 finds it; this also makes
# KDEInstallDirs adopt Qt's multi-arch path scheme. Explicit -DCMAKE_INSTALL_PREFIX
# still wins.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH
        "Install path prefix, prepended onto install directories." FORCE)
endif()

include(KDEInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Core Test)
find_package(KF6Service REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6I18n REQUIRED)

# domain.h is listed so AUTOMOC generates the meta-object for its Q_NAMESPACE
# (the BindingSource enum, exposed to QML by the KCM).
add_library(ads-domain STATIC src/domain.cpp src/domain.h)
target_include_directories(ads-domain PUBLIC src)
target_link_libraries(ads-domain PUBLIC Qt6::Core)

add_library(ads-writer STATIC src/writer.cpp)
target_include_directories(ads-writer PUBLIC src)
target_link_libraries(ads-writer PUBLIC ads-domain KF6::Service KF6::ConfigCore)

# Compile-time enum-coverage guard (ticket 19). ads-domain (componentKey) and
# ads-writer (mimeKeysFor, primaryMimeTypeFor) switch over the full Component
# enum with cases that each return and no empty-return default afterwards. A
# component added to the enum but omitted from one of those switches would
# otherwise compile as a mere -Wswitch warning and silently skip that
# component at run time; promote the missing-case warning to an error so the
# omission fails the build instead. Guarded to GCC/Clang (the CI compiler);
# MSVC reports a missing case via its own switch-unchecked equivalent.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ads-domain PRIVATE -Wswitch -Werror=switch)
    target_compile_options(ads-writer PRIVATE -Wswitch -Werror=switch)
endif()

add_library(ads-switcher STATIC src/switcher.cpp)
target_include_directories(ads-switcher PUBLIC src)
target_link_libraries(ads-switcher PUBLIC ads-domain ads-writer)

add_library(ads-store STATIC src/store.cpp)
target_include_directories(ads-store PUBLIC src)
target_link_libraries(ads-store PUBLIC ads-domain KF6::ConfigCore)

# The shared activity-reader interface. Both the daemon and the KCM program
# against ActivityBackend, so its QObject anchor lives here; the concrete
# KActivities-backed reader stays out of this library so the KCM backend and
# its tests build without the PlasmaActivities/DBus dev packages.
add_library(ads-activities STATIC src/activity_backend.cpp)
target_include_directories(ads-activities PUBLIC src)
target_link_libraries(ads-activities PUBLIC ads-domain Qt6::Core)

# The KCM's testable backend (C++ thinks, QML draws): profile read/write,
# application enumeration, baseline selection and effective-app computation.
# Deliberately free of KCMUtils so it and its tests build wherever the core
# Qt/KF6 packages are; the kcmutils_add_qml_kcm plugin under kcm/ is a thin
# shell over it. Component display names and picker labels are translated with
# the KCM's own domain (translation_domain.h), hence KF6::I18n.
add_library(ads-kcm STATIC
    src/kcm_backend.cpp
    src/component_rows_model.cpp
    src/application_trader_backend.cpp
)
target_include_directories(ads-kcm PUBLIC src)
target_link_libraries(ads-kcm PUBLIC
    ads-domain
    ads-store
    ads-activities
    ads-writer
    KF6::I18n
    KF6::Service
    Qt6::Core
)

target_link_libraries(ads-switcher PUBLIC ads-store)

enable_testing()

add_executable(tst_resolver tests/tst_resolver.cpp)
target_link_libraries(tst_resolver PRIVATE ads-domain Qt6::Test)
add_test(NAME resolver COMMAND tst_resolver)

add_executable(tst_writer tests/tst_writer.cpp)
target_link_libraries(tst_writer PRIVATE ads-writer Qt6::Test)
add_test(NAME writer COMMAND tst_writer)

add_executable(tst_switcher tests/tst_switcher.cpp)
target_link_libraries(tst_switcher PRIVATE ads-switcher ads-writer Qt6::Test)
add_test(NAME switcher COMMAND tst_switcher)

add_executable(tst_store tests/tst_store.cpp)
target_link_libraries(tst_store PRIVATE ads-store Qt6::Test)
add_test(NAME store COMMAND tst_store)

add_executable(tst_kcm tests/tst_kcm.cpp)
target_link_libraries(tst_kcm PRIVATE ads-kcm Qt6::Test)
add_test(NAME kcm COMMAND tst_kcm)

# --- kded6 module (ticket 04) ---
# The KDEDModule base class and the KActivities consumer come from the
# KF6DBusAddons and PlasmaActivities dev packages, installed system-wide via
# `make deps` (scripts/setup-deps.sh) / apt.
find_package(KF6DBusAddons QUIET)
find_package(PlasmaActivities QUIET)

if(KF6DBusAddons_FOUND AND PlasmaActivities_FOUND)
    # The module and its JSON are installed side by side into the kded plugin
    # directory; X-KDE-Kded-autoload makes kded6 load it at session start.
    add_library(activity-defaults-switcher-kded MODULE src/module.cpp src/kactivities_backend.cpp src/module.json)
    target_include_directories(activity-defaults-switcher-kded PRIVATE src)
    target_link_libraries(activity-defaults-switcher-kded PRIVATE
        ads-switcher
        ads-activities
        KF6::DBusAddons
        Plasma::Activities
        KF6::CoreAddons
    )

    install(TARGETS activity-defaults-switcher-kded
        DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/kded)
    install(FILES src/module.json
        DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/kded)

    # kded shell tests (ticket 08): instantiate the module with injected fakes so
    # it never constructs KActivities/D-Bus objects or touches a live config.
    add_executable(tst_module
        tests/tst_module.cpp
        src/module.cpp
        src/kactivities_backend.cpp
    )
    target_include_directories(tst_module PRIVATE src)
    target_link_libraries(tst_module PRIVATE
        ads-switcher
        ads-writer
        ads-store
        ads-activities
        KF6::DBusAddons
        Plasma::Activities
        KF6::CoreAddons
        Qt6::Test
    )
    add_test(NAME module COMMAND tst_module)
endif()

# --- KCM (System Settings module, ticket 07) ---
# A QML KQuickConfigModule (kcm/) over the testable ads-kcm backend. Built only
# when the KCMUtils dev package and the KActivities-backed reader are present;
# ads-kcm and tst_kcm build regardless so the backend stays testable anywhere.
find_package(KF6KCMUtils QUIET)
find_package(Qt6 COMPONENTS Qml Quick QUIET)
if(KF6KCMUtils_FOUND AND Qt6Qml_FOUND AND Qt6Quick_FOUND AND KF6DBusAddons_FOUND AND PlasmaActivities_FOUND)
    add_subdirectory(kcm)

    # --- offscreen QML smoke test (ticket 22) ---
    # tst_kcm pins the KCM's C++ backend; this pins the view (kcm/ui/main.qml)
    # it renders into, which would otherwise ship untested (a broken id, a wrong
    # role, an unresolved enum, a delegate error, a mis-selected combo). It
    # loads the production QML through the real KCMUtils shell: the smoke plugin
    # (tests/kcm_qml/kcm_smoke_module.cpp) is kcm/kcm.cpp's module with an
    # injected KcmBackend. A shared support library is the one address space the
    # test executable and the dlopened plugin both see, so the test hands the
    # plugin the backend built from its fakes.
    add_library(ads-kcm-smoke-support SHARED tests/kcm_qml/kcm_smoke_support.cpp)
    target_include_directories(ads-kcm-smoke-support PUBLIC tests/kcm_qml)
    target_link_libraries(ads-kcm-smoke-support PUBLIC ads-kcm Qt6::Core)

    # A plain MODULE (not kcoreaddons_add_plugin) so the throwaway test plugin
    # is not installed; K_PLUGIN_CLASS_WITH_JSON still supplies the Qt plugin
    # metadata KPluginFactory reads. AUTORCC bundles kcm_smoke.qrc, whose prefix
    # matches the module's plugin id so KQuickConfigModule::mainUi() finds
    # main.qml where the real plugin would have it.
    add_library(ads-kcm-smoke MODULE
        tests/kcm_qml/kcm_smoke_module.cpp
        tests/kcm_qml/kcm_smoke.qrc
    )
    set_target_properties(ads-kcm-smoke PROPERTIES AUTORCC ON)
    target_include_directories(ads-kcm-smoke PRIVATE tests/kcm_qml src)
    target_link_libraries(ads-kcm-smoke PRIVATE
        ads-kcm-smoke-support
        ads-kcm
        KF6::KCMUtilsQuick
        Qt6::Qml
        Qt6::Quick
    )

    add_executable(tst_kcm_qml tests/kcm_qml/tst_kcm_qml.cpp)
    target_include_directories(tst_kcm_qml PRIVATE tests/kcm_qml)
    target_compile_definitions(tst_kcm_qml PRIVATE
        ADS_KCM_SMOKE_PLUGIN="$<TARGET_FILE:ads-kcm-smoke>")
    # The plugin is dlopened at run time, so nothing links it into the test;
    # make building the test build (and place) the plugin it loads.
    add_dependencies(tst_kcm_qml ads-kcm-smoke)
    target_link_libraries(tst_kcm_qml PRIVATE
        ads-kcm-smoke-support
        ads-kcm
        ads-store
        ads-activities
        ads-writer
        ads-domain
        KF6::KCMUtilsQuick
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
    )
    add_test(NAME kcm-qml COMMAND tst_kcm_qml)
endif()

# Install the KCM's translations: KI18N_INSTALL compiles po/<lang>/<catalog>.po
# into .mo under share/locale, where KLocalizedString looks them up. No .po
# files exist yet, so this is a no-op until translations are contributed.
ki18n_install(po)

# --- uninstall ---
# CMake has no built-in uninstall. This custom target removes every file
# recorded in install_manifest.txt (written by `cmake --install`); see
# cmake_uninstall.cmake.in for the removal script.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    @ONLY)
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
