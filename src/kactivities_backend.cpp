// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "kactivities_backend.h"

#include <QtAlgorithms>

namespace ads {

KActivitiesActivityBackend::KActivitiesActivityBackend(QObject *parent)
    : ActivityBackend(parent)
    , m_consumer(new KActivities::Consumer(this))
{
    connect(m_consumer, &KActivities::Consumer::serviceStatusChanged, this,
            [this](KActivities::Consumer::ServiceStatus status) {
                emit runningChanged(status == KActivities::Consumer::Running);
            });
    connect(m_consumer, &KActivities::Consumer::currentActivityChanged, this,
            [this](const QString &id) {
                emit currentActivityChanged(id);
            });
    connect(m_consumer, &KActivities::Consumer::activitiesChanged, this,
            [this](const QStringList &) {
                emit activitiesChanged();
            });
}

KActivitiesActivityBackend::~KActivitiesActivityBackend()
{
    qDeleteAll(m_info);
}

bool KActivitiesActivityBackend::isRunning() const
{
    return m_consumer->serviceStatus() == KActivities::Consumer::Running;
}

QString KActivitiesActivityBackend::currentActivityId() const
{
    return m_consumer->currentActivity();
}

QList<Activity> KActivitiesActivityBackend::allActivities() const
{
    QList<Activity> activities;
    const QStringList ids = m_consumer->activities();
    activities.reserve(ids.size());
    for (const QString &id : ids) {
        activities.append(Activity{id, activityName(id)});
    }
    return activities;
}

QString KActivitiesActivityBackend::activityName(const QString &id) const
{
    KActivities::Info *info = m_info.value(id);
    if (!info) {
        // Unparented: the list is owned here and released in the destructor, so
        // each Info lives exactly as long as this backend. A fresh Info can
        // briefly report an empty name before it synchronizes (KActivities
        // docs); callers that must not mis-select on empty names handle that.
        info = new KActivities::Info(id);
        m_info.insert(id, info);
        // Observe renames (and the initial synchronization of a freshly created
        // Info's empty name) so the module can refresh and re-select the
        // baseline: the Consumer's activitiesChanged does not fire on a rename.
        // activityName is const (an override of the backend's read accessor), so
        // the connection context and the emission use a non-const this - this
        // method already mutates the mutable m_info cache, so the const-ness is
        // accessor-only here. The Info is owned for this backend's lifetime.
        auto *self = const_cast<KActivitiesActivityBackend *>(this);
        QObject::connect(info, &KActivities::Info::nameChanged, self, [self](const QString &) {
            emit self->activitiesRenamed();
        });
    }
    return info->name();
}

} // namespace ads
