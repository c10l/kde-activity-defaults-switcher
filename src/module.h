#pragma once

#include <KDEDModule>

#include <PlasmaActivities/Consumer>
#include <PlasmaActivities/Info>

#include <QHash>
#include <QVariantList>

#include "switcher.h"

namespace ads {

// The auto-loaded kded6 module: it observes the current KDE Activity and, on
// every change, drives the Switcher (resolver + writer) to flip the browser
// default. This is thin glue — all decision logic lives in the resolver, the
// write path in the writer, and the wiring in the Switcher.
class ActivityDefaultsSwitcherModule : public KDEDModule
{
    Q_OBJECT

public:
    explicit ActivityDefaultsSwitcherModule(QObject *parent, const QVariantList &args);

private:
    QString activityName(const QString &id);
    QList<Activity> orderedActivities();
    void refresh();

    KActivities::Consumer *m_consumer = nullptr;
    QHash<QString, KActivities::Info *> m_info;
    Switcher m_switcher;
};

} // namespace ads
