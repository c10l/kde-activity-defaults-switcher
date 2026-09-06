#pragma once

#include "domain.h"

#include <QObject>
#include <QString>

namespace ads {

// The observable KDE activity layer the kded shell is driven by. Keeping it
// behind this small seam lets the shell glue (signal -> refresh/capture,
// baseline selection, running-state gating) be unit-tested with a fake that is
// told to report a running state and to emit its own change signals, without
// constructing the D-Bus-bound KActivities::Consumer/Info objects.
class ActivityBackend : public QObject
{
    Q_OBJECT

public:
    // Constructed once; the concrete backend supplies all external behaviour.
    explicit ActivityBackend(QObject *parent = nullptr);
    ~ActivityBackend() override;

    // True once the activities service is usable (KActivities::Consumer::Running).
    virtual bool isRunning() const = 0;
    // The current (focused) activity id. Empty when not running.
    virtual QString currentActivityId() const = 0;
    // Every existing activity, with its name looked up per id.
    virtual QList<Activity> allActivities() const = 0;
    // The display name of one activity id (may be empty before the underlying
    // info synchronizes).
    virtual QString activityName(const QString &id) const = 0;

Q_SIGNALS:
    // The activities service started (or stopped). True means Running.
    void runningChanged(bool running);
    // The current (focused) activity changed.
    void currentActivityChanged(const QString &id);
    // The set of activities changed (e.g. one created or removed).
    void activitiesChanged();
};

} // namespace ads
