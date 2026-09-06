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
    }
    return info->name();
}

} // namespace ads
