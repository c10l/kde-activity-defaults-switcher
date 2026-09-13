// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#pragma once

#include "kcm_backend.h"

#include <KQuickConfigModule>

namespace ads {

// The KCM's shell: a KQuickConfigModule over a KcmBackend (C++ thinks, QML
// draws) that owns the module's lifecycle wiring. It is the single copy of that
// wiring shared by the production plugin (kcm/kcm.cpp) and the offscreen QML
// smoke test's plugin (tests/kcm_qml/kcm_smoke_module.cpp), so a change to the
// shell - the backend property, the org.kde.ads enum registration, the Apply
// semantics, load()/save() - fails the smoke test the way it would fail System
// Settings. Only createBackend() differs: production wires the real KActivities
// reader and KApplicationTrader enumeration, the smoke test wires fakes.
class KcmShell : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(ads::KcmBackend *backend READ backend CONSTANT)

public:
    KcmShell(QObject *parent, const KPluginMetaData &metaData);
    ~KcmShell() override;

    ads::KcmBackend *backend();

    // Snapshot load: the KCM reads the activities and profiles once, when the
    // shell loads the module (and on the shell's own reset). A load discards
    // any buffered edits in the backend.
    void load() override;

    // Apply: persist the buffered edits. The daemon's store watch then flips the
    // live default (ADR 0002).
    void save() override;

protected:
    // Builds the backend the shell exposes. Called once, lazily on the first
    // backend() read (not from the constructor, where the derived override is
    // not yet in place). The implementation decides ownership: production
    // parents the new backend to the shell, the smoke test returns a backend it
    // owns.
    virtual ads::KcmBackend *createBackend() = 0;

private:
    ads::KcmBackend *m_backend = nullptr;
};

} // namespace ads
