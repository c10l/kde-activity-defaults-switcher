// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#pragma once

#include "activity_backend.h"

#include <PlasmaActivities/Consumer>
#include <PlasmaActivities/Info>

#include <QHash>

namespace ads {

// The production ActivityBackend: reads/watches the real KDE activity layer
// through KActivities::Consumer. Activity names are resolved via lazily-created,
// per-id KActivities::Info objects owned by this backend (released on destruction).
class KActivitiesActivityBackend : public ActivityBackend
{
    Q_OBJECT

public:
    explicit KActivitiesActivityBackend(QObject *parent = nullptr);
    ~KActivitiesActivityBackend() override;

    bool isRunning() const override;
    QString currentActivityId() const override;
    QList<Activity> allActivities() const override;

private:
    QString activityName(const QString &id) const override;

    KActivities::Consumer *m_consumer = nullptr;
    mutable QHash<QString, KActivities::Info *> m_info;
};

} // namespace ads
