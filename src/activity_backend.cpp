#include "activity_backend.h"

namespace ads {

// Out-of-line anchor so the meta-object (signals, vtable) for the abstract
// ActivityBackend is emitted exactly once, from this TU. The class API is
// otherwise pure virtual; only construction/destruction live here.
ActivityBackend::ActivityBackend(QObject *parent)
    : QObject(parent)
{
}

ActivityBackend::~ActivityBackend() = default;

} // namespace ads

#include "activity_backend.moc"
