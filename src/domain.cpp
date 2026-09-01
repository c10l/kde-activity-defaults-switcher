#include "domain.h"

namespace ads {

QList<Component> allComponents()
{
    return {
        Component::Browser,
        Component::Email,
        Component::FileManager,
        Component::TextEditor,
        Component::Calendar,
        Component::Geo,
        Component::ImageViewer,
        Component::MusicPlayer,
        Component::PdfViewer,
        Component::Tel,
        Component::VideoPlayer,
        Component::ArchiveManager,
        Component::Terminal,
    };
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

std::optional<Activity> Resolver::selectBaselineActivity(const QList<Activity> &orderedActivities)
{
    for (const Activity &activity : orderedActivities) {
        if (activity.name == QStringLiteral("Default")) {
            return activity;
        }
    }
    if (!orderedActivities.isEmpty()) {
        return orderedActivities.first();
    }
    return std::nullopt;
}

QMap<Component, Application> Resolver::resolve(
    const Activity &currentActivity,
    const std::optional<Activity> &baselineActivity,
    const QMap<QString, Profile> &profiles,
    const QMap<Component, Application> &currentDefaults) const
{
    const Profile currentProfile = profiles.value(currentActivity.name);
    const Profile baselineProfile = baselineActivity.has_value()
        ? profiles.value(baselineActivity->name)
        : Profile{};

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
