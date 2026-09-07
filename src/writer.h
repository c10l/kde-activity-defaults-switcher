// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#pragma once

#include "domain.h"

namespace ads {

// The mime keys each component manages, exactly as the System Settings
// component chooser defines them (plasma-workspace kcms/componentchooser),
// plus text/html for the browser so .html files and links both follow the
// activity. The terminal component has no mime keys: it is stored in
// kdeglobals instead.
QStringList mimeKeysFor(Component component);

// The canonical mime type a component's "current default" is read through -
// the same key the System Settings component chooser uses to load it.
QString primaryMimeTypeFor(Component component);

// The current default application of every mime component, read directly from
// the mimeapps.list file the writer writes (ADR-0001) - not through
// KApplicationTrader, which serves from the KSycoca cache and can lag a fresh
// edit. Empty when the file has no entry for a component.
QMap<Component, Application> readCurrentMimeDefaults();

// The current default terminal application, read from kdeglobals [General]
// TerminalService (ADR-0001). Empty when unset.
Application readCurrentTerminalDefault();

// The current default application of every component: the mime components from
// mimeapps.list plus the terminal from kdeglobals. This is the map the switcher
// resolves against and the writer's no-op check consumes. Empty for components
// with no configured default.
QMap<Component, Application> readCurrentDefaults();

// The Writer applies resolved decisions to the system default configuration.
// It is a pure write seam: given resolved decisions and the current defaults,
// it writes each mime component's application via
// KApplicationTrader::setPreferredService and the terminal via kdeglobals
// (TerminalService/TerminalApplication, ADR-0001). A component whose decision
// already equals the current default is not rewritten.
class Writer
{
public:
    virtual ~Writer() = default;

    // Writes each decision to the XDG mime configuration, skipping components
    // whose decision already equals the current default, components without
    // known mime keys, empty applications, and a terminal decision whose storage
    // id resolves to no desktop entry (which would write an empty
    // TerminalApplication and break "Open Terminal", ADR-0001). The terminal
    // component is written to kdeglobals instead (TerminalService/
    // TerminalApplication, ADR-0001). Returns the number of components written.
    virtual int apply(const QMap<Component, Application> &decisions,
                      const QMap<Component, Application> &currentDefaults) const;
};

} // namespace ads
