#!/usr/bin/env bash
# setup-workspace.sh
#
# Requires: kdotool (https://github.com/jinliu/kdotool): xdotool-like tool
# for KDE Plasma that drives KWin's scripting API over DBus.
#
# Checks that the CURRENT virtual desktop has zero windows assigned to it
# (including minimized ones), and only then opens 3 Konsole windows and
# places them top-right / bottom-left / bottom-right of the panel-aware
# usable area (top-left intentionally left empty).
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
check_script="
${desktop_lookup_js}
var blockers = workspace.windowList().filter(function(w) {
    if (!w.normalWindow) return false;
    if (w.onAllDesktops) return true;
    return w.desktops.indexOf(cur) !== -1;
});
if (blockers.length > 0) {
    output_error('not empty: ' + blockers.length + ' window(s) on this desktop');
} else {
    output_result('empty');
}
"

if ! check_out=$(kdotool kwinscript --inline "$check_script" 2>&1); then
    echo "Current virtual desktop is not empty, aborting." >&2
    echo "$check_out" >&2
    exit 1
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
