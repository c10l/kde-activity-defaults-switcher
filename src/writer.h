#pragma once

#include "domain.h"

namespace ads {

// The Writer applies resolved decisions to the XDG mime configuration. It is
// a pure write seam: given resolved decisions and the current defaults, it
// writes each component's application via
// KApplicationTrader::setPreferredService. A component whose decision already
// equals the current default is not rewritten. The terminal component is out
// of scope here (see ticket 06).
class Writer
{
public:
    virtual ~Writer() = default;

    // Writes each decision to the XDG mime configuration, skipping components
    // whose decision already equals the current default, components without
    // known mime keys, and empty applications. Returns the number of
    // components written.
    virtual int apply(const QMap<Component, Application> &decisions,
                      const QMap<Component, Application> &currentDefaults) const;
};

} // namespace ads
