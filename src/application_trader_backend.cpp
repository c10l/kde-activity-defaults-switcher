// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "application_trader_backend.h"

#include "writer.h"

#include <KApplicationTrader>
#include <KService>
#include <QSet>

namespace ads {

namespace {

// The themed icon a picker entry shows for an Application. A desktop entry that
// names no icon gets a generic one, so every entry keeps its icon slot (the
// upstream Default Applications picker does the same).
QString iconFor(const KService::Ptr &service)
{
    if (service && !service->icon().isEmpty()) {
        return service->icon();
    }
    return QStringLiteral("application-x-executable");
}

} // namespace

QList<ApplicationCandidate> ApplicationTraderBackend::applicationsFor(Component component) const
{
    KService::List services;
    if (component == Component::Terminal) {
        // No KServiceTypeTrader in KF6: filter the TerminalEmulator category.
        services = KApplicationTrader::query([](const KService::Ptr &service) {
            return service->categories().contains(QStringLiteral("TerminalEmulator"));
        });
    } else {
        const QString mimeType = primaryMimeTypeFor(component);
        if (mimeType.isEmpty()) {
            return {};
        }
        services = KApplicationTrader::queryByMimeType(mimeType);
    }

    // The storageId is the value a Binding persists (domain.h); drop duplicates
    // so a component sharing a desktop entry across categories appears once.
    QList<ApplicationCandidate> candidates;
    QSet<Application> seen;
    for (const KService::Ptr &service : services) {
        const Application storageId = service->storageId();
        if (storageId.isEmpty() || seen.contains(storageId)) {
            continue;
        }
        seen.insert(storageId);
        candidates.append(ApplicationCandidate{storageId, service->name(), iconFor(service)});
    }
    return candidates;
}

QString ApplicationTraderBackend::displayNameFor(const Application &application) const
{
    const KService::Ptr service = KService::serviceByStorageId(application);
    return service ? service->name() : application;
}

QString ApplicationTraderBackend::iconNameFor(const Application &application) const
{
    return iconFor(KService::serviceByStorageId(application));
}

} // namespace ads
