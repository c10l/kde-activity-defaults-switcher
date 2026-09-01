#pragma once

#include "domain.h"

namespace ads {

// The ProfileStore persists profiles in a KConfig file with the agreed
// structure (spec): one group per component, activity id as key, storageId as
// value. It is a stateless file seam, testable against a temporary config
// directory like the writer.
class ProfileStore
{
public:
    explicit ProfileStore(const QString &fileName = QStringLiteral("activity-defaults-switcherrc"));
    virtual ~ProfileStore() = default;

    // Reads every profile from the store. Missing files yield an empty map.
    virtual QMap<ActivityId, Profile> load() const;

    // Persists exactly the given profiles, replacing any previously stored
    // bindings. Profiles without bindings leave no entries behind.
    virtual void save(const QMap<ActivityId, Profile> &profiles) const;

private:
    QString m_fileName;
};

} // namespace ads
