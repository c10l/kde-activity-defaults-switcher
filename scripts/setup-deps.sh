#!/usr/bin/env bash
# Stages the KDE development headers + CMake package configs needed to build
# the kded6 module into ./deps (gitignored), without touching the system.
#
# The runtime shared libraries (libKF6DBusAddons, libPlasmaActivities) are
# already installed system-wide; only the corresponding -dev packages are
# missing in this environment. This downloads and extracts just those -dev
# packages and points their versioned .so import locations at the system
# libraries (which match the same versions).
set -euo pipefail

cd "$(dirname "$0")/.."

if command -v apt-get >/dev/null 2>&1; then
    apt_get=apt-get
elif command -v apt >/dev/null 2>&1; then
    apt_get=apt
else
    echo "error: neither apt-get nor apt found" >&2
    exit 1
fi

mkdir -p deps
cd deps

pkgs=(libkf6dbusaddons-dev libplasmaactivities-dev kded6-dev)
for pkg in "${pkgs[@]}"; do
    if ! compgen -G "${pkg}_*.deb" >/dev/null; then
        "$apt_get" download "$pkg"
    fi
done

for deb in ./*.deb; do
    dpkg-deb -x "$deb" .
done

# The imported CMake targets reference the fully-versioned .so shipped in the
# runtime packages. Symlink those from the system libraries.
libdir=usr/lib/x86_64-linux-gnu
mkdir -p "$libdir"

link_system_libs() {
    local prefix="$1"
    local found=0
    local f
    for f in "/usr/lib/x86_64-linux-gnu/${prefix}.so."*; do
        [ -e "$f" ] || continue
        ln -sf "$f" "$libdir/$(basename "$f")"
        found=1
    done
    if [ "$found" -eq 0 ]; then
        echo "warning: no system ${prefix}.so.* found" >&2
    fi
}

link_system_libs libKF6DBusAddons
link_system_libs libPlasmaActivities

echo "KDE dev deps staged in deps/ (headers, CMake package configs, lib symlinks)."
