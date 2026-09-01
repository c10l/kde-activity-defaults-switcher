#include "switcher.h"

namespace ads {

Switcher::Switcher(std::unique_ptr<Resolver> resolver, std::unique_ptr<Writer> writer)
    : m_resolver(resolver ? std::move(resolver) : std::make_unique<Resolver>())
    , m_writer(writer ? std::move(writer) : std::make_unique<Writer>())
{
}

int Switcher::switchActivity(const Activity &currentActivity,
                             const std::optional<Activity> &baselineActivity,
                             const QMap<ActivityId, Profile> &profiles,
                             const QMap<Component, Application> &currentDefaults)
{
    const QMap<Component, Application> decisions =
        m_resolver->resolve(currentActivity, baselineActivity, profiles, currentDefaults);

    // Tracer bullet: only the browser component is applied in this ticket.
    QMap<Component, Application> toApply;
    if (const auto it = decisions.constFind(Component::Browser); it != decisions.constEnd()) {
        toApply.insert(Component::Browser, it.value());
    }
    return m_writer->apply(toApply, currentDefaults);
}

} // namespace ads
