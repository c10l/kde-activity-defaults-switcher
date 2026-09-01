#pragma once

#include "domain.h"

namespace ads {

// The mime keys each component manages, exactly as the System Settings
// component chooser defines them (plasma-workspace kcms/componentchooser),
// plus text/html for the browser so .html files and links both follow the
// activity. The terminal component has no mime keys: it is stored in
// kdeglobals (ticket 06).
QStringList mimeKeysFor(Component component);

// The canonical mime type a component's "current default" is read through -
// the same key the System Settings component chooser uses to load it.
QString primaryMimeTypeFor(Component component);

// The current default application of every mime component, read directly from
// the mimeapps.list file the writer writes (ADR-0001) - not through
// KApplicationTrader, which serves from the KSycoca cache and can lag a fresh
// edit. Empty when the file has no entry for a component.
QMap<Component, Application> readCurrentMimeDefaults();

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
