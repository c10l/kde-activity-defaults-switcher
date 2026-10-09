// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#pragma once

#include "domain.h"

#include <optional>

namespace ads {

// Asks the user for an Application to bind to a Component - the picker's
// "Other…" entry, for an Application the Component's own enumeration does not
// offer (one that does not advertise the Component's mime type, or any other the
// user wants). Behind this interface so the KCM backend is unit-tested with a
// fake and never opens a dialog; the production implementation is
// KOpenWithDialog (kcm/kopenwithdialog_chooser.cpp), the same chooser the System
// Settings Default Applications page uses.
class ApplicationChooser
{
public:
    virtual ~ApplicationChooser() = default;

    // The chosen Application's storageId, or nullopt when the user cancelled -
    // or chose something that is not an Application (a bare command), which a
    // Binding cannot hold.
    virtual std::optional<Application> choose(Component component) = 0;
};

} // namespace ads
