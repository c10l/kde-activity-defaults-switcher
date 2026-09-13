# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 c10l

# kcoreaddons_add_plugin (inside kcmutils_add_qml_kcm) requires
# CMAKE_LIBRARY_OUTPUT_DIRECTORY, normally provided by ECM's KDECMakeSettings.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# The QML KCM plugin, a thin shell over the ads-kcm backend. kcmutils_add_qml_kcm
# requires the plugin JSON to share the target's basename (the plugin id, which
# is also the translation domain) and bundles the ui/ directory into the target's
# qrc. The KActivities-backed reader is compiled in here (rather than pulled from
# ads-activities) so the backend library itself needs no PlasmaActivities.
kcmutils_add_qml_kcm(activity-defaults-switcher-kcm
    SOURCES
        kcm.cpp
        kcm_shell.cpp
        ${CMAKE_SOURCE_DIR}/src/kactivities_backend.cpp
)
target_include_directories(activity-defaults-switcher-kcm PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(activity-defaults-switcher-kcm PRIVATE
    ads-kcm
    KF6::KCMUtilsQuick
    KF6::I18n
    Plasma::Activities
    Qt6::Qml
    Qt6::Quick
)
