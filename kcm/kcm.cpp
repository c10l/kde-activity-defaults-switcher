// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "application_trader_backend.h"
#include "kactivities_backend.h"
#include "kcm_backend.h"

#include <KPluginFactory>
#include <KQuickConfigModule>
#include <QQmlEngine>

#include <memory>

using namespace ads;

// The System Settings module: a thin QML KQuickConfigModule over the testable
// KcmBackend (C++ thinks, QML draws). It wires the production dependencies -
// the real KActivities reader and the KApplicationTrader enumeration - and
// forwards the shell lifecycle to the backend.
class ActivityDefaultsSwitcherKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(ads::KcmBackend *backend READ backend CONSTANT)

public:
    ActivityDefaultsSwitcherKcm(QObject *parent, const KPluginMetaData &metaData)
        : KQuickConfigModule(parent, metaData),
          m_backend(new KcmBackend(std::make_unique<KActivitiesActivityBackend>(),
                                   std::make_unique<ApplicationTraderBackend>(),
                                   std::make_unique<ProfileStore>(), this))
    {
        // Apply-button contract: edits are buffered in the backend and written to
        // the store only when the user clicks Apply (the shell calls save()).
        // This is the rest of System Settings' behaviour and the opposite of the
        // KAbstractConfigModule default (instant apply), so pin it explicitly:
        // an upstream default change must not silently revert the KCM to
        // applying on every edit. The Apply button is enabled from needsSave,
        // driven by the backend's pending-changes signal.
        setSupportsInstantApply(false);
        connect(m_backend, &KcmBackend::hasPendingChangesChanged, this,
                [this] { setNeedsSave(m_backend->hasPendingChanges()); });

        // Expose the domain's BindingSource enum to QML (the component rows'
        // bindingSource role), so the view can mark fallback bindings. The URI
        // is registered once per process.
        static const int registered = qmlRegisterUncreatableMetaObject(
            ads::staticMetaObject, "org.kde.ads", 1, 0, "Ads",
            QStringLiteral("Ads is an enum namespace and cannot be created"));
        Q_UNUSED(registered);
    }

    ads::KcmBackend *backend() const { return m_backend; }

    // Snapshot load: the KCM reads the activities and profiles once, when the
    // shell loads the module (and on the shell's own reset). A load discards
    // any buffered edits in the backend.
    void load() override { m_backend->load(); }

    // Apply: persist the buffered edits. The daemon's store watch then flips the
    // live default (ADR 0002).
    void save() override { m_backend->save(); }

private:
    ads::KcmBackend *m_backend;
};

K_PLUGIN_CLASS_WITH_JSON(ActivityDefaultsSwitcherKcm, "activity-defaults-switcher-kcm.json")

#include "kcm.moc"
