// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#pragma once

namespace ads {

// The KCM's translation domain. KQuickConfigModule sets the QML engine's i18n
// domain to the plugin id, so QML i18n()/i18nc() resolve against the catalog
// named here; C++ strings must pass the same domain explicitly to
// i18nd/i18ndc, because the process-wide domain belongs to System Settings.
// Must match the kcmutils_add_qml_kcm target / plugin JSON basename
// (kcm/activity-defaults-switcher-kcm.json).
inline constexpr char kcmTranslationDomain[] = "activity-defaults-switcher-kcm";

} // namespace ads
