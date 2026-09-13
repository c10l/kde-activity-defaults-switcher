// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "kcm_shell.h"

#include "domain.h"

#include <KPluginMetaData>
#include <QQmlEngine>
#include <QString>

using namespace ads;

KcmShell::KcmShell(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
{
    // Apply-button contract: edits are buffered in the backend and written to
    // the store only when the user clicks Apply (the shell calls save()). This
    // is the rest of System Settings' behaviour and the opposite of the
    // KAbstractConfigModule default (instant apply), so pin it explicitly: an
    // upstream default change must not silently revert the KCM to applying on
    // every edit. The Apply button is enabled from needsSave, driven by the
    // backend's pending-changes signal (connected where the backend is built).
    setSupportsInstantApply(false);

    // Expose the domain's BindingSource enum to QML (the component rows'
    // bindingSource role), so the view can mark fallback bindings. The URI is
    // registered once per process.
    static const int registered = qmlRegisterUncreatableMetaObject(
        ads::staticMetaObject, "org.kde.ads", 1, 0, "Ads",
        QStringLiteral("Ads is an enum namespace and cannot be created"));
    Q_UNUSED(registered);
}

KcmShell::~KcmShell() = default;

ads::KcmBackend *KcmShell::backend()
{
    // Lazily built on the first read (the QML property is CONSTANT, so it is
    // read once per instance): createBackend() is virtual and must not be
    // called from the constructor, where the derived override is not yet in
    // place.
    if (!m_backend) {
        m_backend = createBackend();
        connect(m_backend, &KcmBackend::hasPendingChangesChanged, this,
                [this] { setNeedsSave(m_backend->hasPendingChanges()); });
    }
    return m_backend;
}

void KcmShell::load()
{
    backend()->load();
}

void KcmShell::save()
{
    backend()->save();
}
