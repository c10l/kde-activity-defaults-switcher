#pragma once

#include "domain.h"
#include "writer.h"

#include <memory>

namespace ads {

// The Switcher is the kded shell: it wires an activity change through the
// resolver and applies the result via the writer. For this tracer bullet only
// the browser component is applied. It owns its resolver and writer (both
// injectable for tests), and knows nothing of KDE, D-Bus, or the filesystem —
// the kded module feeds it the current state.
class Switcher
{
public:
    explicit Switcher(std::unique_ptr<Resolver> resolver = {},
                      std::unique_ptr<Writer> writer = {});

    // Resolves the browser decision for `currentActivity` and applies it,
    // returning the number of components written (0 or 1 here). The
    // `baselineActivity` must already be selected (see
    // Resolver::selectBaselineActivity).
    int switchActivity(const Activity &currentActivity,
                       const std::optional<Activity> &baselineActivity,
                       const QMap<ActivityId, Profile> &profiles,
                       const QMap<Component, Application> &currentDefaults);

private:
    std::unique_ptr<Resolver> m_resolver;
    std::unique_ptr<Writer> m_writer;
};

} // namespace ads
