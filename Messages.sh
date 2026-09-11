#!/bin/sh
# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 c10l
#
# Extract the KCM's translatable strings into its catalog. Run by the KDE
# translation pipeline (extract-messages.sh / Scripty), not by the build:
# `make` only compiles any committed po/ catalogs via ki18n_install(po).
#
# $EXTRACTRC and $XGETTEXT are provided by the pipeline; $podir is the catalog
# output directory.

# Strings embedded in .ui/.rc/.kcfg resources (none yet, but the standard set).
lst=$(find src kcm -name '*.rc' -o -name '*.ui' -o -name '*.kcfg')
if [ -n "$lst" ]; then
    $EXTRACTRC $lst >> rc.cpp
fi

# C++ (i18nd/i18ndc) and QML (i18n/i18nc) strings all land in the KCM catalog,
# whose name is the plugin id / translation domain.
$XGETTEXT $(find src kcm -name '*.cpp' -o -name '*.h' -o -name '*.qml') \
    -o "$podir/activity-defaults-switcher-kcm.pot"

rm -f rc.cpp
