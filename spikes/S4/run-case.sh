#!/usr/bin/env bash
# Run one S4 case in the nested session: watch windows with watch.js while
# s4launch launches the given specs, then print both outputs merged by time.
#
# Usage: VDW_DEV_DIR=... spikes/S4/run-case.sh OUTDIR NAME [s4launch args...]
# Env: S4_WATCH (seconds to watch, default 12), S4_EXTRA (a command run via
# dev-session.sh run in the background 0.3 s after the launch starts).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
ds=$here/../../scripts/dev-session.sh
outdir=$1 name=$2
shift 2
mkdir -p "$outdir"

VDW_SCRIPT_WAIT=${S4_WATCH:-12} "$ds" script "$here/watch.js" >"$outdir/$name.watch" 2>&1 &
watch_pid=$!
sleep 1
if [[ -n ${S4_EXTRA:-} ]]; then
    (sleep ${S4_EXTRA_DELAY:-0.3}; "$ds" run -- sh -c "echo s4 \$(date +%s%3N) extra-start cmd=\"$S4_EXTRA\"; exec $S4_EXTRA" >"$outdir/$name.extra" 2>&1 </dev/null) >/dev/null 2>&1 &
fi
"$ds" run -- "$here/build/s4launch" "$@" >"$outdir/$name.launch" 2>"$outdir/$name.launch.err" || true
wait "$watch_pid" || true
grep -h -E '^s4(win)? [0-9]+' "$outdir/$name.launch" "$outdir/$name.watch" "$outdir/$name.extra" 2>/dev/null | sort -k2,2n || true
