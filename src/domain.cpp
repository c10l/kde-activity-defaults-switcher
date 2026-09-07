// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "domain.h"

#include <QtGlobal>

#include <algorithm>

namespace ads {

QList<Component> allComponents()
{
    // Derived from the enum's contiguous default values over [0, Count): a
    // component added to the enum above Count is picked up automatically, so
    // this list cannot silently omit a member the way a hand-maintained
    // literal could (the very drift ticket 19 exists to prevent). Count itself
    // is a count sentinel, never a component, and is excluded by the range.
    QList<Component> components;
    constexpr int count = static_cast<int>(Component::Count);
    components.reserve(count);
    for (int value = 0; value < count; ++value) {
        components.append(static_cast<Component>(value));
    }
    return components;
}

void Profile::setBinding(Component component, const Application &application)
{
    m_bindings.insert(component, application);
}

void Profile::removeBinding(Component component)
{
    m_bindings.remove(component);
}

std::optional<Application> Profile::bindingFor(Component component) const
{
    const auto it = m_bindings.constFind(component);
    if (it == m_bindings.constEnd()) {
        return std::nullopt;
    }
    return it.value();
}

QList<Binding> Profile::bindings() const
{
    QList<Binding> result;
    result.reserve(m_bindings.size());
    for (auto it = m_bindings.constBegin(); it != m_bindings.constEnd(); ++it) {
        result.append(Binding{it.key(), it.value()});
    }
    return result;
}

bool Profile::hasBinding(Component component) const
{
    return m_bindings.contains(component);
}

bool Profile::isEmpty() const
{
    return m_bindings.isEmpty();
}

QString componentKey(Component component)
{
    switch (component) {
    case Component::Browser:
        return QStringLiteral("Browser");
    case Component::Email:
        return QStringLiteral("Email");
    case Component::FileManager:
        return QStringLiteral("FileManager");
    case Component::TextEditor:
        return QStringLiteral("TextEditor");
    case Component::Calendar:
        return QStringLiteral("Calendar");
    case Component::Geo:
        return QStringLiteral("Geo");
    case Component::ImageViewer:
        return QStringLiteral("ImageViewer");
    case Component::MusicPlayer:
        return QStringLiteral("MusicPlayer");
    case Component::PdfViewer:
        return QStringLiteral("PdfViewer");
    case Component::Tel:
        return QStringLiteral("Tel");
    case Component::VideoPlayer:
        return QStringLiteral("VideoPlayer");
    case Component::ArchiveManager:
        return QStringLiteral("ArchiveManager");
    case Component::Terminal:
        return QStringLiteral("Terminal");
    case Component::Count:
        // The trailing count sentinel is never a component.
        break;
    }
    Q_UNREACHABLE();
}

QList<Activity> orderedActivityList(const QList<Activity> &activities)
{
    QList<Activity> ordered = activities;
    std::sort(ordered.begin(), ordered.end(), [](const Activity &a, const Activity &b) {
        const int nameCompare = QString::compare(a.name, b.name, Qt::CaseInsensitive);
        if (nameCompare != 0) {
            return nameCompare < 0;
        }
        return a.id < b.id;
    });
    return ordered;
}

std::optional<Activity> Resolver::selectBaselineActivity(const QList<Activity> &activities)
{
    const QList<Activity> ordered = orderedActivityList(activities);
    for (const Activity &activity : ordered) {
        if (activity.name == QStringLiteral("Default")) {
            return activity;
        }
    }
    if (!ordered.isEmpty()) {
        return ordered.first();
    }
    return std::nullopt;
}

QMap<Component, Application>
Resolver::resolve(const Activity &currentActivity, const std::optional<Activity> &baselineActivity,
                  const QMap<ActivityId, Profile> &profiles,
                  const QMap<Component, Application> &currentDefaults) const
{
    const Profile currentProfile = profiles.value(currentActivity.id);
    const Profile baselineProfile =
        baselineActivity.has_value() ? profiles.value(baselineActivity->id) : Profile{};

    QMap<Component, Application> decisions;
    for (const Component component : allComponents()) {
        if (const auto binding = currentProfile.bindingFor(component)) {
            decisions.insert(component, *binding);
            continue;
        }
        if (const auto binding = baselineProfile.bindingFor(component)) {
            decisions.insert(component, *binding);
            continue;
        }
        const Application currentDefault = currentDefaults.value(component);
        if (!currentDefault.isEmpty()) {
            decisions.insert(component, currentDefault);
        }
    }
    return decisions;
}

} // namespace ads
