#!/usr/bin/env bash
# S2 spike: authorize the test helpers (s2-fakeinput, s2-shot) in the NESTED
# session only, by writing .desktop files into its isolated XDG_DATA_HOME.
# KWin grants org_kde_kwin_fake_input and org.kde.KWin.ScreenShot2 to
# executables listed in such files. Never point this at the live session.
#
# Usage: VDW_DEV_DIR=... spikes/S2/nested-authorize.sh BUILD_DIR
set -euo pipefail
build=$(realpath "${1:?usage: $0 BUILD_DIR}")
state=${VDW_DEV_DIR:?set VDW_DEV_DIR to the nested session state dir}
apps=$state/home/.local/share/applications
[[ -d $state/home ]] || { echo "no nested session home in $state" >&2; exit 1; }
mkdir -p "$apps"
cat >"$apps/vdw-s2-fakeinput.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=vdw S2 fake input (nested test helper)
Exec=$build/s2-fakeinput
NoDisplay=true
X-KDE-Wayland-Interfaces=org_kde_kwin_fake_input
EOF
cat >"$apps/vdw-s2-shot.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=vdw S2 screenshot (nested test helper)
Exec=$build/s2-shot
NoDisplay=true
X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2
EOF
echo "wrote $apps/vdw-s2-fakeinput.desktop and vdw-s2-shot.desktop"
