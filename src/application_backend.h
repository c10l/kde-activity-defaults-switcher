// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#pragma once

#include "domain.h"

#include <QList>
#include <QString>

namespace ads {

// One Application the picker can offer: its storageId plus the name to display
// (already translated by the desktop entry where one exists).
struct ApplicationCandidate {
    Application storageId;
    QString displayName;
};

// Enumerates the Applications a Component's picker can offer. Behind this
// interface so the KCM backend is unit-tested with a fake and never calls
// KApplicationTrader.
class ApplicationBackend
{
public:
    virtual ~ApplicationBackend() = default;

    // The candidate Applications for `component`, preference-sorted (most
    // preferred first). Empty when none are installed.
    virtual QList<ApplicationCandidate> applicationsFor(Component component) const = 0;

    // The display name of an Application, used for the
    // "Inherit (baseline: <name>)" picker entry. Falls back to the storageId
    // when no desktop entry is known.
    virtual QString displayNameFor(const Application &application) const = 0;
};

} // namespace ads
