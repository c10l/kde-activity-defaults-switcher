// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "application_trader_backend.h"
#include "kactivities_backend.h"
#include "kcm_shell.h"
#include "kopenwithdialog_chooser.h"

#include <KPluginFactory>

#include <memory>

using namespace ads;

// The System Settings module: the shared KcmShell (all the lifecycle wiring)
// over the production dependencies - the real KActivities reader, the
// KApplicationTrader enumeration, and the KOpenWithDialog "Other…" chooser. The
// smoke test's plugin is the same shell over fakes, so the shell wiring has one
// copy (kcm/kcm_shell.cpp).
class ActivityDefaultsSwitcherKcm : public KcmShell
{
    Q_OBJECT

public:
    ActivityDefaultsSwitcherKcm(QObject *parent, const KPluginMetaData &metaData)
        : KcmShell(parent, metaData)
    {
    }

protected:
    ads::KcmBackend *createBackend() override
    {
        return new KcmBackend(std::make_unique<KActivitiesActivityBackend>(),
                              std::make_unique<ApplicationTraderBackend>(),
                              std::make_unique<ProfileStore>(),
                              std::make_unique<KOpenWithDialogChooser>(), this);
    }
};

K_PLUGIN_CLASS_WITH_JSON(ActivityDefaultsSwitcherKcm, "activity-defaults-switcher-kcm.json")

#include "kcm.moc"
