// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#pragma once

#include "application_backend.h"

namespace ads {

// The production ApplicationBackend: enumerates Applications through
// KApplicationTrader, exactly as the System Settings component chooser does.
// The 12 mime components are queried by their primary mime type (preference
// sorted, Flatpak entries included via KSycoca); the terminal is filtered by
// the TerminalEmulator category (there is no KServiceTypeTrader in KF6).
class ApplicationTraderBackend : public ApplicationBackend
{
public:
    QList<ApplicationCandidate> applicationsFor(Component component) const override;
    QString displayNameFor(const Application &application) const override;
    QString iconNameFor(const Application &application) const override;
};

} // namespace ads
