// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

// The KQuickConfigModule shell the offscreen QML smoke test (ticket 22) loads:
// it is kcm/kcm.cpp's module with the production KActivities/KApplicationTrader
// backend swapped for the test-injected one (kcm_smoke_support.h), so the view
// renders deterministic row states. Everything else - the QML resource, the
// KCMUtils shell, the domain enum registration - is the production machinery,
// so a QML regression (broken id, wrong role, unresolved enum, delegate error,
// mis-selected combo) fails the test the way it would fail System Settings.

#include "kcm_smoke_support.h"

#include "domain.h"
#include "kcm_backend.h"

#include <KPluginFactory>
#include <KQuickConfigModule>
#include <QQmlEngine>

using namespace ads;

class KcmSmokeModule : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(ads::KcmBackend *backend READ backend CONSTANT)

public:
    KcmSmokeModule(QObject *parent, const KPluginMetaData &metaData)
        : KQuickConfigModule(parent, metaData)
    {
        // The production module registers this in kcm/kcm.cpp; the view needs
        // the BindingSource enum (Ads.BindingSource.Fallback) resolved. Once
        // per process.
        static const int registered = qmlRegisterUncreatableMetaObject(
            ads::staticMetaObject, "org.kde.ads", 1, 0, "Ads",
            QStringLiteral("Ads is an enum namespace and cannot be created"));
        Q_UNUSED(registered);
    }

    ads::KcmBackend *backend() const { return kcm_smoke::backend(); }

    void load() override
    {
        if (auto *b = kcm_smoke::backend()) {
            b->load();
        }
    }
};

K_PLUGIN_CLASS_WITH_JSON(KcmSmokeModule, "kcm_smoke_module.json")

#include "kcm_smoke_module.moc"
