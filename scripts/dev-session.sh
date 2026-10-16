#!/usr/bin/env bash
# dev-session.sh: a headless, nested KWin session for development and tests.
#
# Starts kwin_wayland with the virtual backend inside its own D-Bus session,
# with its own XDG_RUNTIME_DIR, HOME and XDG config/data/cache/state dirs, so
# nothing it does (or anything launched into it) reaches the live desktop or
# the user's real config files.
#
# Usage:
#   scripts/dev-session.sh start [--no-xwayland] [--width W] [--height H]
#   scripts/dev-session.sh stop
#   scripts/dev-session.sh status
#   scripts/dev-session.sh env [--fish]     print exports for the nested session
#   scripts/dev-session.sh run -- CMD...    run CMD against the nested session
#   scripts/dev-session.sh script FILE.js   load and run a KWin script, print
#                                           what it logged
#   scripts/dev-session.sh selftest         check scripting, Konsole, kglobalaccel
#   scripts/dev-session.sh log              print the KWin log path
#
# State lives in $VDW_DEV_DIR (default: ${TMPDIR:-$XDG_RUNTIME_DIR}/vdw-dev-session).
# The default is short on purpose: Unix socket paths are limited to 107 bytes.

set -euo pipefail

SOCKET_NAME=wayland-vdw
DEFAULT_WIDTH=1920
DEFAULT_HEIGHT=1080
START_TIMEOUT=20

base_tmp=${TMPDIR:-${XDG_RUNTIME_DIR:-/tmp}}
STATE_DIR=${VDW_DEV_DIR:-$base_tmp/vdw-dev-session}
RUNTIME_DIR=$STATE_DIR/runtime
HOME_DIR=$STATE_DIR/home
PID_FILE=$STATE_DIR/pid
ENV_FILE=$STATE_DIR/session.env
LOG_FILE=$STATE_DIR/kwin.log

die() {
    printf 'dev-session: %s\n' "$*" >&2
    exit 1
}

info() {
    printf 'dev-session: %s\n' "$*" >&2
}

is_running() {
    [[ -f $PID_FILE ]] || return 1
    local pid
    pid=$(<"$PID_FILE")
    [[ -n $pid ]] && kill -0 "$pid" 2>/dev/null || return 1
    # Guard against a stale pid file whose pid got reused: stop signals the
    # whole process group, so make sure it is still our dbus-run-session.
    # Right after start the pid briefly still shows as setsid or env, before
    # they exec into dbus-run-session, so accept those too.
    case $(cat "/proc/$pid/comm" 2>/dev/null) in
        dbus-run-sessio* | setsid | env) return 0 ;;
        *) return 1 ;;
    esac
}

# Environment for processes inside the nested session. Printed as
# NAME=VALUE lines; DISPLAY may be empty when Xwayland is off or failed.
session_vars() {
    [[ -f $ENV_FILE ]] || die "no session environment; is the session started?"
    # shellcheck disable=SC1090
    (
        . "$ENV_FILE"
        printf 'DBUS_SESSION_BUS_ADDRESS=%s\n' "$DBUS_SESSION_BUS_ADDRESS"
        printf 'WAYLAND_DISPLAY=%s\n' "$WAYLAND_DISPLAY"
        printf 'DISPLAY=%s\n' "${DISPLAY:-}"
        printf 'XDG_RUNTIME_DIR=%s\n' "$RUNTIME_DIR"
        printf 'HOME=%s\n' "$HOME_DIR"
        printf 'XDG_CONFIG_HOME=%s\n' "$HOME_DIR/.config"
        printf 'XDG_DATA_HOME=%s\n' "$HOME_DIR/.local/share"
        printf 'XDG_CACHE_HOME=%s\n' "$HOME_DIR/.cache"
        printf 'XDG_STATE_HOME=%s\n' "$HOME_DIR/.local/state"
        printf 'XDG_SESSION_TYPE=wayland\n'
        printf 'XDG_CURRENT_DESKTOP=KDE\n'
        printf 'KDE_FULL_SESSION=true\n'
        printf 'KDE_SESSION_VERSION=6\n'
        printf 'QT_QPA_PLATFORM=wayland\n'
    )
}

cmd_env() {
    is_running || die "session is not running (start it with: $0 start)"
    local fish=0
    [[ ${1:-} == --fish ]] && fish=1
    local line name value
    while IFS= read -r line; do
        name=${line%%=*}
        value=${line#*=}
        if [[ $name == DISPLAY && -z $value ]]; then
            if ((fish)); then echo "set -e DISPLAY"; else echo "unset DISPLAY"; fi
            continue
        fi
        if ((fish)); then
            printf 'set -gx %s %q\n' "$name" "$value"
        else
            printf 'export %s=%q\n' "$name" "$value"
        fi
    done < <(session_vars)
}

cmd_run() {
    [[ ${1:-} == -- ]] && shift
    (($# > 0)) || die "usage: $0 run -- CMD [ARGS...]"
    is_running || die "session is not running (start it with: $0 start)"
    local -a assigns=()
    local line display=""
    while IFS= read -r line; do
        case $line in
            DISPLAY=*) display=${line#DISPLAY=} ;;
            *) assigns+=("$line") ;;
        esac
    done < <(session_vars)
    if [[ -n $display ]]; then
        exec env "${assigns[@]}" DISPLAY="$display" "$@"
    else
        exec env -u DISPLAY "${assigns[@]}" "$@"
    fi
}

wait_ready() {
    local deadline=$((SECONDS + START_TIMEOUT))
    while ((SECONDS < deadline)); do
        is_running || return 1
        if [[ -s $ENV_FILE ]]; then
            local addr
            addr=$(. "$ENV_FILE" && echo "$DBUS_SESSION_BUS_ADDRESS")
            if dbus-send --bus="$addr" --print-reply --dest=org.freedesktop.DBus \
                /org/freedesktop/DBus org.freedesktop.DBus.NameHasOwner \
                string:org.kde.KWin 2>/dev/null | grep -q 'boolean true'; then
                return 0
            fi
        fi
        sleep 0.2
    done
    return 1
}

cmd_start() {
    local xwayland=1 width=$DEFAULT_WIDTH height=$DEFAULT_HEIGHT
    while (($# > 0)); do
        case $1 in
            --no-xwayland) xwayland=0 ;;
            --width) width=$2; shift ;;
            --height) height=$2; shift ;;
            *) die "unknown option for start: $1" ;;
        esac
        shift
    done

    if is_running; then
        info "already running (pid $(<"$PID_FILE")); state in $STATE_DIR"
        return 0
    fi

    command -v kwin_wayland >/dev/null || die "kwin_wayland not found"
    command -v dbus-run-session >/dev/null || die "dbus-run-session not found"

    rm -rf "$RUNTIME_DIR"
    rm -f "$ENV_FILE" "$PID_FILE"
    mkdir -p "$STATE_DIR" "$RUNTIME_DIR" "$HOME_DIR/.config" "$HOME_DIR/.local/share" \
        "$HOME_DIR/.cache" "$HOME_DIR/.local/state"
    chmod 700 "$STATE_DIR" "$RUNTIME_DIR"

    # KWin starts this once Wayland (and Xwayland, if enabled) is up, with
    # WAYLAND_DISPLAY and DISPLAY set. It records the environment and exits.
    local hook=$STATE_DIR/ready-hook.sh
    cat >"$hook" <<EOF
#!/bin/sh
{
    printf 'DBUS_SESSION_BUS_ADDRESS=%s\n' "\$DBUS_SESSION_BUS_ADDRESS"
    printf 'WAYLAND_DISPLAY=%s\n' "\$WAYLAND_DISPLAY"
    printf 'DISPLAY=%s\n' "\${DISPLAY:-}"
} > '$ENV_FILE.tmp'
mv '$ENV_FILE.tmp' '$ENV_FILE'
EOF
    chmod 700 "$hook"

    # Own bus config: listen on a socket path inside our runtime dir. The
    # default session.conf uses unix:tmpdir=/tmp, which may pick an abstract
    # socket (refused in sandboxes) or write to /tmp.
    local busconf=$STATE_DIR/session-bus.conf
    cat >"$busconf" <<EOF
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <keep_umask/>
  <listen>unix:path=$RUNTIME_DIR/bus</listen>
  <auth>EXTERNAL</auth>
  <standard_session_servicedirs/>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
EOF

    local -a kwin_args=(--virtual --width "$width" --height "$height" --no-lockscreen
        --socket "$SOCKET_NAME")
    ((xwayland)) && kwin_args+=(--xwayland)
    kwin_args+=("$hook")

    : >"$LOG_FILE"
    # setsid: own process group, so stop can take down KWin, the bus and
    # everything launched into the session together.
    setsid env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
        -u SESSION_MANAGER -u XAUTHORITY \
        XDG_RUNTIME_DIR="$RUNTIME_DIR" \
        HOME="$HOME_DIR" \
        XDG_CONFIG_HOME="$HOME_DIR/.config" \
        XDG_DATA_HOME="$HOME_DIR/.local/share" \
        XDG_CACHE_HOME="$HOME_DIR/.cache" \
        XDG_STATE_HOME="$HOME_DIR/.local/state" \
        XDG_SESSION_TYPE=wayland \
        XDG_CURRENT_DESKTOP=KDE \
        KDE_FULL_SESSION=true \
        KDE_SESSION_VERSION=6 \
        QT_QPA_PLATFORM=wayland \
        QT_LOGGING_RULES="kwin_scripting.debug=true" \
        QT_FORCE_STDERR_LOGGING=1 \
        dbus-run-session --config-file="$busconf" -- kwin_wayland "${kwin_args[@]}" \
        >>"$LOG_FILE" 2>&1 </dev/null &
    echo $! >"$PID_FILE"

    if ! wait_ready; then
        if is_running; then
            info "KWin did not come up within ${START_TIMEOUT}s; last log lines:"
        else
            info "KWin exited during startup; last log lines:"
        fi
        tail -n 20 "$LOG_FILE" >&2 || true
        cmd_stop >/dev/null 2>&1 || true
        exit 1
    fi

    local display
    display=$(. "$ENV_FILE" && echo "${DISPLAY:-}")
    info "started (pid $(<"$PID_FILE")), ${width}x${height}"
    info "state: $STATE_DIR"
    info "log:   $LOG_FILE"
    if ((xwayland)) && [[ -z $display ]]; then
        info "warning: Xwayland requested but DISPLAY is not set (see log)"
    fi
    cmd_env
}

cmd_stop() {
    if ! is_running; then
        info "not running"
        rm -f "$PID_FILE" "$ENV_FILE"
        return 0
    fi
    local pid
    pid=$(<"$PID_FILE")
    # The background job was started with setsid, so its pid is the pgid.
    kill -TERM -- "-$pid" 2>/dev/null || kill -TERM "$pid" 2>/dev/null || true
    local i
    for ((i = 0; i < 50; i++)); do
        kill -0 "$pid" 2>/dev/null || break
        sleep 0.1
    done
    if kill -0 "$pid" 2>/dev/null; then
        info "still alive after 5s, sending SIGKILL"
        kill -KILL -- "-$pid" 2>/dev/null || kill -KILL "$pid" 2>/dev/null || true
    fi
    # Clients started with `run` live in the caller's process group; they lose
    # their Wayland and D-Bus connections here and exit on their own.
    rm -f "$PID_FILE" "$ENV_FILE"
    rm -rf "$RUNTIME_DIR"
    info "stopped"
}

cmd_status() {
    if is_running; then
        echo "running (pid $(<"$PID_FILE")), state in $STATE_DIR"
    else
        echo "not running"
        return 1
    fi
}

# Load FILE as a KWin script in the nested session, run it, print the lines
# it logged (print()/console.log() go to KWin's stderr, i.e. the log), then
# unload it.
cmd_script() {
    local file=${1:-}
    [[ -f $file ]] || die "usage: $0 script FILE.js"
    is_running || die "session is not running (start it with: $0 start)"
    file=$(realpath "$file")
    local name
    name="vdw-dev-$(basename "$file" .js)-$$"
    local before
    before=$(wc -l <"$LOG_FILE")

    local addr
    addr=$(. "$ENV_FILE" && echo "$DBUS_SESSION_BUS_ADDRESS")
    local q=(env DBUS_SESSION_BUS_ADDRESS="$addr" qdbus6)
    local id
    id=$("${q[@]}" org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$file" "$name")
    [[ $id =~ ^[0-9]+$ ]] || die "loadScript failed: $id"
    "${q[@]}" org.kde.KWin "/Scripting/Script$id" org.kde.kwin.Script.run >/dev/null
    sleep "${VDW_SCRIPT_WAIT:-0.5}"
    "${q[@]}" org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript "$name" >/dev/null || true
    # Scripts print() through the kwin_scripting logging category (enabled
    # in start); show only what was logged since the script was loaded.
    tail -n "+$((before + 1))" "$LOG_FILE" | sed -e 's/^js: //' -e 's/^kwin_scripting: //'
}

# Check the acceptance criteria end to end: session up, KWin scripting over
# D-Bus, a Konsole launched into the session is seen by a KWin script and by
# kdotool, and whether kglobalaccel is on the nested bus.
cmd_selftest() {
    local started_here=0 failures=0
    if ! is_running; then
        cmd_start >/dev/null
        started_here=1
    fi

    local addr kwin_pid
    addr=$(. "$ENV_FILE" && echo "$DBUS_SESSION_BUS_ADDRESS")
    local q=(env DBUS_SESSION_BUS_ADDRESS="$addr" qdbus6)

    report() {
        if [[ $1 == ok ]]; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; failures=$((failures + 1)); fi
    }

    if "${q[@]}" org.kde.KWin /Scripting >/dev/null 2>&1; then
        report ok "org.kde.KWin /Scripting reachable on nested bus"
    else
        report fail "org.kde.KWin /Scripting reachable on nested bus"
    fi

    kwin_pid=$(pgrep -g "$(<"$PID_FILE")" -x kwin_wayland | head -n 1 || true)
    local accel_pid
    accel_pid=$("${q[@]}" org.freedesktop.DBus / org.freedesktop.DBus.GetConnectionUnixProcessID \
        org.kde.kglobalaccel 2>/dev/null || true)
    if [[ -n $accel_pid ]]; then
        report ok "org.kde.kglobalaccel owned by pid $accel_pid (kwin_wayland is pid ${kwin_pid:-?})"
    else
        report fail "org.kde.kglobalaccel present on nested bus"
    fi

    local tmp
    tmp=$(mktemp -d "$STATE_DIR/selftest.XXXXXX")
    "$0" run -- konsole --separate >"$tmp/konsole.log" 2>&1 &
    local konsole_pid=$!
    local found=0 i
    cat >"$tmp/list-windows.js" <<'EOF'
const windows = workspace.windowList();
for (const w of windows) {
    if (w.normalWindow) {
        print("vdw-selftest window class=" + w.resourceClass + " caption=" + w.caption
              + " geometry=" + w.frameGeometry.x + "," + w.frameGeometry.y + " "
              + w.frameGeometry.width + "x" + w.frameGeometry.height);
    }
}
print("vdw-selftest done count=" + windows.length);
EOF
    for ((i = 0; i < 30; i++)); do
        sleep 0.5
        # Subshell: cmd_script exits on errors, which must not end selftest.
        if (cmd_script "$tmp/list-windows.js") >"$tmp/script.out" 2>&1 \
            && grep -q 'vdw-selftest window class=org.kde.konsole' "$tmp/script.out"; then
            found=1
            break
        fi
    done
    grep 'vdw-selftest' "$tmp/script.out" | sed 's/^/      /' || true
    if ((found)); then
        report ok "KWin script sees the Konsole window"
    else
        report fail "KWin script sees the Konsole window (see $tmp)"
    fi

    if command -v kdotool >/dev/null; then
        local ids
        ids=$("$0" run -- kdotool search --class konsole 2>&1 || true)
        if [[ $ids =~ \{ ]]; then
            report ok "kdotool search --class konsole: $(echo "$ids" | tr '\n' ' ')"
        else
            report fail "kdotool search --class konsole: $ids"
        fi
    fi

    kill "$konsole_pid" 2>/dev/null || true
    wait "$konsole_pid" 2>/dev/null || true
    ((started_here)) && cmd_stop >/dev/null 2>&1
    ((failures == 0))
}

main() {
    local cmd=${1:-}
    (($# > 0)) && shift
    case $cmd in
        start) cmd_start "$@" ;;
        stop) cmd_stop ;;
        status) cmd_status ;;
        env) cmd_env "$@" ;;
        run) cmd_run "$@" ;;
        script) cmd_script "$@" ;;
        selftest) cmd_selftest ;;
        log) echo "$LOG_FILE" ;;
        *)
            sed -n '2,/^$/s/^# \{0,1\}//p' "$0" >&2
            exit 2
            ;;
    esac
}

main "$@"
