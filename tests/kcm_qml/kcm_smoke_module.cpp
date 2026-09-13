// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

// The offscreen QML smoke test's plugin (ticket 22): the *shared* production
// shell (kcm/kcm_shell.cpp) with the production KActivities/KApplicationTrader
// backend swapped for the test-injected one (kcm_smoke_support.h), so the view
// renders deterministic row states. Because the shell is shared rather than
// copied, a change to the shell's wiring - the backend property, the org.kde.ads
// enum registration, the Apply semantics and the pending-changes connection,
// load()/save() - fails the smoke test the way it would fail System Settings;
// only createBackend() differs here.

#include "kcm_smoke_support.h"

#include "kcm_backend.h"
#include "kcm_shell.h"

#include <KPluginFactory>

using namespace ads;

class KcmSmokeModule : public KcmShell
{
    Q_OBJECT

public:
    KcmSmokeModule(QObject *parent, const KPluginMetaData &metaData) : KcmShell(parent, metaData) {}

protected:
    ads::KcmBackend *createBackend() override { return kcm_smoke::backend(); }
};

K_PLUGIN_CLASS_WITH_JSON(KcmSmokeModule, "kcm_smoke_module.json")

#include "kcm_smoke_module.moc"
