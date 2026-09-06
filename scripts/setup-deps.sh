#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 c10l

# Installs the Debian packages the build needs.
#
# The KF6 / Plasma / Qt6 development packages are installed system-wide via
# apt (same approach the CI workflow uses). Run this on a Debian system with
# root to provision a full build environment in one step.
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
    echo "error: must be run as root (needs apt-get)" >&2
    exit 1
fi

pkgs=(
    ca-certificates
    build-essential
    cmake
    git
    extra-cmake-modules
    qt6-base-dev
    libkf6service-dev
    libkf6config-dev
    libkf6coreaddons-dev
    libkf6dbusaddons-dev
    libplasmaactivities-dev
    kded6-dev
    clang-format
    clang-tidy
)

apt-get update
DEBIAN_FRONTEND=noninteractive \
    apt-get install -y --no-install-recommends "${pkgs[@]}"

echo "KDE dev packages installed system-wide."
