#!/usr/bin/env bash
# Spike S3 driver helpers. Everything goes through dev-session.sh, i.e. the
# nested session in $VDW_DEV_DIR (default /run/user/1000/vdw-s3), never the
# live session.
set -euo pipefail
export VDW_DEV_DIR=${VDW_DEV_DIR:-/run/user/1000/vdw-s3}
here=$(cd "$(dirname "$0")" && pwd)
ds=$here/../../scripts/dev-session.sh
run() { "$ds" run -- "$@"; }
k() { run qdbus6 org.kde.KWin "$@"; }
s3() { run qdbus6 org.kde.vdw.S3 /S3 "org.kde.vdw.S3.$1" "${@:2}"; }

cmd=$1
shift
case $cmd in
    load) # load NAME FILE: load and run a script, print its id
        id=$(k /Scripting org.kde.kwin.Scripting.loadScript "$(realpath "$2")" "$1")
        echo "id=$id"
        k "/Scripting/Script$id" org.kde.kwin.Script.run
        ;;
    unload) k /Scripting org.kde.kwin.Scripting.unloadScript "$1" ;;
    loaded) k /Scripting org.kde.kwin.Scripting.isScriptLoaded "$1" ;;
    poke) run qdbus6 org.kde.kglobalaccel /component/kwin org.kde.kglobalaccel.Component.invokeShortcut "VDW S3 Poke" ;;
    work) s3 QueueWork "$1" ;;
    pending) s3 AddPending "$@" ;;
    s3) s3 "$@" ;;
    k) k "$@" ;;
    launch) # launch CMD...: start an app in the nested session, detached
        setsid -f "$ds" run -- "$@" >>"$VDW_DEV_DIR/apps.log" 2>&1 </dev/null
        ;;
    log) tail -n "${1:-40}" "$VDW_DEV_DIR/receiver.log" ;;
    *) echo "unknown: $cmd" >&2; exit 2 ;;
esac
