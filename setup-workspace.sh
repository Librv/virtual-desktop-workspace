#!/usr/bin/env bash
# setup-workspace.sh
#
# Requires: kdotool (https://github.com/jinliu/kdotool): xdotool-like tool
# for KDE Plasma that drives KWin's scripting API over DBus.
#
# Checks whether the CURRENT virtual desktop has zero windows assigned to
# it (including minimized ones). If so, opens 3 Konsole windows and places
# them top-right / bottom-left / bottom-right of the panel-aware usable
# area (top-left intentionally left empty).
#
# If the desktop is busy, nothing launches on the first press: a Plasma OSD
# message asks to tap the shortcut again. A second press within
# CONFIRM_TIMEOUT_MS on the same desktop sets up anyway, alongside the
# existing windows (they are left untouched; placement is unchanged).
#
# Bind this script to a global shortcut via System Settings > Shortcuts >
# Custom Shortcuts (Command/URL trigger). It must run as a normal process
# (not a pure KWin script) because KWin scripts cannot spawn processes.
#
# --- Self-logging (always on) ------------------------------------------
# Everything below is logged unconditionally to a fixed file, regardless
# of how this script gets invoked (terminal, gio launch, or the real
# global shortcut via kglobalaccel). This exists because kglobalaccel
# appears to cache the invoked command at shortcut-registration time
# rather than re-reading the .desktop file's Exec= on every press, which
# made wrapping this in "sh -c '... > logfile'" at the .desktop level
# unreliable. Remove the next two lines once you're done debugging.

#exec >>/tmp/setup-workspace.log 2>&1
#set -x

echo "=== run at $(date -Ins) ==="
set -euo pipefail

# --- Busy-desktop confirm settings ---------------------------------------
# A pending request is a per-user state file holding "<epoch ms> <desktop>".
# XDG_RUNTIME_DIR is a per-user tmpfs cleared at logout; in the fallback
# directories the uid in the file name keeps it per-user.
CONFIRM_TIMEOUT_MS=2000
CONFIRM_MSG="Desktop not empty. Tap D again to set up here."
OSD_ICON="preferences-desktop-virtual"
STATE_FILE="${XDG_RUNTIME_DIR:-${TMPDIR:-/tmp}}/setup-workspace-$(id -u).pending"

command -v kdotool >/dev/null || { echo "kdotool not found. Install it first." >&2; exit 1; }
command -v konsole >/dev/null || { echo "konsole not found." >&2; exit 1; }

# --- 0. Pin the target desktop up front ---------------------------------
# Captured once as an x11 desktop number (kdotool's own get_desktop),
# then every KWin script below looks up *this specific* desktop by number
# instead of re-reading workspace.currentDesktop live. Without this, if
# you switch virtual desktops while the script is still polling for new
# windows, later steps silently start comparing against whatever desktop
# is current *then* instead of the one this run actually targets, which
# is what was causing an unrelated window on a different desktop to get
# grabbed and moved.
TARGET_DESKTOP=$(kdotool get_desktop)
echo "TARGET_DESKTOP=$TARGET_DESKTOP"

desktop_lookup_js="var cur = workspace.desktops.find(function(d){ return d.x11DesktopNumber == ${TARGET_DESKTOP}; });"

# --- 1. Emptiness check --------------------------------------------------
# A window "counts" if it's a normal application window assigned to the
# target desktop (or pinned to all desktops), regardless of minimized
# state. Panels/docks/desktop widgets are excluded via normalWindow.
# Reports "empty" or "busy <n>". A failing kdotool call is a hard error,
# so a broken check never turns into a confirm prompt.
check_script="
${desktop_lookup_js}
var blockers = workspace.windowList().filter(function(w) {
    if (!w.normalWindow) return false;
    if (w.onAllDesktops) return true;
    return w.desktops.indexOf(cur) !== -1;
});
output_result(blockers.length > 0 ? 'busy ' + blockers.length : 'empty');
"

check_out=$(kdotool kwinscript --inline "$check_script")
echo "check: $check_out"

# Best effort: the OSD is only feedback, so a missing plasmashell (e.g. in
# a nested test session) is logged and ignored.
show_osd() {
    timeout 2 qdbus6 org.kde.plasmashell /org/kde/osdService \
        org.kde.osdService.showText "$OSD_ICON" "$1" >/dev/null ||
    timeout 2 gdbus call --session --dest org.kde.plasmashell \
        --object-path /org/kde/osdService \
        --method org.kde.osdService.showText "$OSD_ICON" "$1" >/dev/null ||
    echo "OSD unavailable, message was: $1" >&2
}

# --- 1b. Busy desktop: first press arms, second press confirms ----------
# The read-decide-write on the state file runs under flock, so presses that
# arrive together are handled one after another and only one of them can
# consume a pending request (a triple tap gives one setup, not two). A
# request that is too old, for another desktop, from the future (clock
# change) or unreadable counts as absent and is just overwritten, so stale
# files need no separate cleanup.
if [[ "$check_out" != empty ]]; then
    exec 9>>"$STATE_FILE.lock"
    flock 9
    now_ms=$(date +%s%3N)
    pending_ms="" pending_desktop=""
    read -r pending_ms pending_desktop 2>/dev/null < "$STATE_FILE" || true
    if [[ "$pending_ms" =~ ^[0-9]+$ && "$pending_desktop" == "$TARGET_DESKTOP" ]] &&
       (( now_ms >= pending_ms && now_ms - pending_ms <= CONFIRM_TIMEOUT_MS )); then
        rm -f "$STATE_FILE"
        exec 9>&-
        echo "Confirmed after $(( now_ms - pending_ms )) ms, setting up alongside existing windows."
    else
        echo "$now_ms $TARGET_DESKTOP" > "$STATE_FILE"
        exec 9>&-
        echo "Desktop $TARGET_DESKTOP is busy ($check_out), waiting for a confirm press."
        show_osd "$CONFIRM_MSG"
        exit 0
    fi
fi

# --- 2. Panel-aware usable geometry for this desktop/screen -------------
geom_script="
${desktop_lookup_js}
var area = workspace.clientArea(KWin.PlacementArea, workspace.activeScreen, cur);
output_result(Math.round(area.x) + ' ' + Math.round(area.y) + ' ' + Math.round(area.width) + ' ' + Math.round(area.height));
"
read -r AX AY AW AH <<< "$(kdotool kwinscript --inline "$geom_script")"
echo "geometry: AX=$AX AY=$AY AW=$AW AH=$AH"

# Each window gets HALF the usable width/height, so the 3 placed windows
# tile as flush quadrants (top-left quadrant intentionally left empty).
WW=$(( AW / 2 ))
WH=$(( AH / 2 ))

# --- 3. Launch all 3 in parallel, then place by window-ID diff ----------
# Snapshot existing window IDs on the *pinned* target desktop first,
# launch all 3 Konsoles at once, then poll until 3 new IDs show up in the
# same listing and place those. IDs are KWin's internal per-window UUID
# (the same one kdotool itself uses), valid as an explicit window-id
# argument to windowmove/windowsize.
list_ids_script="
${desktop_lookup_js}
var ids = workspace.windowList().filter(function(w) {
    return w.normalWindow && (w.onAllDesktops || w.desktops.indexOf(cur) !== -1);
}).map(function(w) { return w.internalId.toString(); });
output_result(ids.join(' '));
"
list_ids() { kdotool kwinscript --inline "$list_ids_script"; }
sort_ids() { tr ' ' '\n' | grep -v '^$' | sort; }

before=$(list_ids)
echo "before=$before"

konsole &
konsole &
konsole &

new_ids=""
tries=0
while true; do
    now=$(list_ids)
    new_ids=$(comm -13 <(printf '%s' "$before" | sort_ids) <(printf '%s' "$now" | sort_ids))
    count=$(grep -c . <<< "$new_ids" 2>/dev/null || true)
    if (( count >= 3 )); then break; fi
    tries=$(( tries + 1 ))
    if (( tries > 100 )); then
        echo "Timed out: only found $count new window(s) after launching 3." >&2
        break
    fi
    sleep 0.05
done
echo "new_ids=$new_ids"

mapfile -t ids <<< "$new_ids"
id1=${ids[0]:-} id2=${ids[1]:-} id3=${ids[2]:-}

if [[ -z "$id1" || -z "$id2" || -z "$id3" ]]; then
    echo "Could not identify 3 new windows to place, aborting placement." >&2
    exit 1
fi

place() {
    local id=$1 x=$2 y=$3 w=$4 h=$5
    kdotool windowmove "$id" "$x" "$y"
    kdotool windowsize "$id" "$w" "$h"
}

# Order among the 3 doesn't matter: they're identical fresh Konsoles.
place "$id1" $(( AX + AW - WW )) "$AY" "$WW" "$WH" &            # top-right
place "$id2" "$AX" $(( AY + AH - WH )) "$WW" "$WH" &            # bottom-left
place "$id3" $(( AX + AW - WW )) $(( AY + AH - WH )) "$WW" "$WH" &  # bottom-right

wait
echo "=== done ==="
