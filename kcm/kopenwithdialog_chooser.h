// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#pragma once

#include "application_chooser.h"

namespace ads {

// The production ApplicationChooser: KOpenWithDialog, the same chooser the
// System Settings Default Applications page uses for its "Other…" entry, so an
// Application the Component's own enumeration does not offer can still be bound.
//
// It lives here, in the KCM plugin, rather than in ads-kcm: it is a Widgets/KIO
// dialog, and the KCM backend stays free of both (its tests inject a fake).
class KOpenWithDialogChooser : public ApplicationChooser
{
public:
    std::optional<Application> choose(Component component) override;
};

} // namespace ads
