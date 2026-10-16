# T0b findings: dev environment

Date: 2026-10-17. Machine: CachyOS, KWin 6.7.5, KF6 6.31, Qt 6.12, CMake 4.4,
GCC 16.2.

## Summary

| Item | Status |
|---|---|
| `scripts/dev-session.sh` (start/stop/status/env/run/script/selftest) | written; could not be run end to end from the agent sandbox (see below) |
| KWin scripting over D-Bus in the nested session | not verified (sandbox); `selftest` checks it |
| Konsole launched into the nested session, seen by a KWin script | not verified (sandbox); `selftest` checks it |
| kglobalaccel in the nested session | expected to live inside KWin (see below); `selftest` checks it |
| CMake skeleton, Qt Test target, CTest | builds and passes with a temporary ECM and Makefiles |
| `cmake --preset dev` as written | fails: `extra-cmake-modules` and `ninja` are not installed |
| `.clang-format` | copied verbatim from ECM 6.31 `kde-modules/clang-format.cmake` |
| shellcheck | not installed; only `bash -n` was run |

## Blocker 1: the agent sandbox forbids Unix sockets

Claude Code's Bash sandbox on this machine refuses `socket(AF_UNIX, ...)`
outright, not just for some paths:

```
$ python3 -I -c "import socket; socket.socket(socket.AF_UNIX)"
PermissionError: [Errno 1] Operation not permitted
```

So inside the sandbox nothing can create a D-Bus bus or a Wayland socket:

```
$ scripts/dev-session.sh start
dev-session: KWin did not come up within 20s; last log lines:
dbus-daemon[13]: Failed to start message bus: Failed to open socket: Operation not permitted
dbus-run-session: EOF reading address from bus daemon
```

This also rules out `kdotool`/`qdbus6` against any bus, including the nested
one. The agent did not bypass the sandbox. To let agents use the nested
session the user has to pick one of these via `/sandbox` (or
`~/.claude/settings.json`):

1. **Recommended:** add `scripts/dev-session.sh` to `sandbox.excludedCommands`.
   Agents then run nested work as a single command, e.g.
   `scripts/dev-session.sh start`, `scripts/dev-session.sh selftest`,
   `scripts/dev-session.sh run -- ctest --preset dev`. Only those commands
   leave the sandbox, and they always get the nested environment (own bus,
   own `WAYLAND_DISPLAY`, own `HOME`/XDG dirs, `DISPLAY` unset). Everything
   else stays unable to open sockets, so it cannot reach the live session.
2. Allow all Unix sockets in the sandbox (`allowAllUnixSockets`). Simpler,
   but then any sandboxed command can also connect to the live session's
   `/run/user/1000/bus` and `wayland-0` (the read-only mount does not stop
   `connect()`), which is exactly what the project rules want to avoid.

Until then, nested-session checks are steps for the user (below).

## Blocker 2: build prerequisites missing

`extra-cmake-modules` (ECM) and `ninja` are not installed, so:

```
$ cmake --preset dev
CMake Error: CMake was unable to find a build program corresponding to "Ninja".
```

The user needs to install them (Arch: `extra-cmake-modules ninja`;
`shellcheck` too, for linting the scripts). The agent did not install
anything system-wide. To check the CMake files anyway, ECM 6.31.0 was fetched
from invent.kde.org and installed into `$TMPDIR/ecm-prefix` (outside the
repo, throwaway), then:

```
$ cmake --preset dev -G "Unix Makefiles" -B $TMPDIR/vdw-build-make \
      -DCMAKE_PREFIX_PATH=$TMPDIR/ecm-prefix
-- The following REQUIRED packages have been found:
 * ECM (required version >= 6.10.0)
 * Qt6 (required version >= 6.8.0)
-- Configuring done (0.8s)
$ cmake --build $TMPDIR/vdw-build-make
[100%] Built target sanitytest
$ ctest --test-dir $TMPDIR/vdw-build-make --output-on-failure
1/2 Test #1: appstreamtest ....................   Passed
2/2 Test #2: sanitytest .......................   Passed
100% tests passed out of 2
```

`appstreamtest` comes from ECM's `KDECMakeSettings` and is harmless.
`kde_clang_format` is wired up, so `cmake --build --preset dev --target
clang-format` formats `src/` and `autotests/`. Note that `KDEClangFormat`
rewrites `.clang-format` on configure while it still carries ECM's "do not
edit" header; the committed file is identical to what it writes, so this is a
no-op. Remove the header line if the style ever needs local changes.

## How the nested session is set up

`scripts/dev-session.sh start`:

- State in `$VDW_DEV_DIR`, default `${TMPDIR:-$XDG_RUNTIME_DIR}/vdw-dev-session`
  (pid, `session.env`, `kwin.log`, `runtime/` with mode 0700 used as
  `XDG_RUNTIME_DIR`, `home/` used as `HOME` and for all `XDG_*_HOME`). The
  path is kept short because Unix socket paths are limited to 107 bytes; the
  repo path is already ~90 bytes, so the repo's `.cache/` would not fit.
- Its own D-Bus config listening on `unix:path=$runtime/bus`, because the
  stock `session.conf` uses `unix:tmpdir=/tmp` (abstract socket or `/tmp`).
  Service activation uses the standard session service dirs; activated
  services inherit the isolated environment with `DISPLAY` unset.
- `setsid env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS ...
  dbus-run-session --config-file=... -- kwin_wayland --virtual --width 1920
  --height 1080 --no-lockscreen --socket wayland-vdw --xwayland <hook>`.
  The hook is passed as KWin's "application to start once Wayland and
  Xwayland are up"; it writes the bus address, `WAYLAND_DISPLAY` and
  `DISPLAY` to `session.env`, which doubles as the readiness signal. `start`
  then waits until `org.kde.KWin` is owned on the nested bus.
- Isolating `HOME` and the XDG dirs matters: without it the nested KWin and
  anything launched into it would read and write the user's real
  `~/.config/kwinrc`, `kglobalshortcutsrc`, Konsole profiles and so on.
- `stop` sends SIGTERM to the whole process group (KWin, dbus-daemon, the
  hook and anything KWin started), then SIGKILL after 5 s. It checks that the
  pid file still points at `dbus-run-session` before signalling.
- `script FILE.js` loads a KWin script through `org.kde.KWin /Scripting
  loadScript`, runs it, unloads it and prints what it logged.
  `QT_LOGGING_RULES=kwin_scripting.debug=true` is set so `print()` output
  reaches `kwin.log`.

Unverified assumptions to check on the first real run: that the virtual
backend renders without a GPU in this setup; that Xwayland can create its
socket (it needs `/tmp/.X11-unix`, which outside the sandbox is fine); the
exact prefix KWin puts on `print()` lines (the script prints all new lines,
filtered of `js: ` / `kwin_scripting: ` prefixes, so any format works).

## kglobalaccel

`ldd /usr/lib/libkwin.so.6` shows `libKGlobalAccelD.so.0`, and there is no
`org.kde.kglobalaccel.service` D-Bus activation file. So on Wayland,
kglobalacceld runs inside KWin and `org.kde.kglobalaccel` should be owned by
the nested `kwin_wayland` (unless it is started with
`--no-global-shortcuts`). The standalone `/usr/lib/kglobalacceld` with the
`KGlobalAccelDXcb` platform plugin is for X11 sessions. `selftest` prints the
owner pid next to KWin's pid.

Gap for S1 (expected, not yet tested): the virtual backend has no keyboard,
so real press/release of Ctrl+Meta+D cannot be produced in the nested
session. `org.kde.kglobalaccel.Component.invokeShortcut` triggers the action
but gives no release, so it is not enough for tap/hold. Options to look at
in S1: KWin's fake-input Wayland protocol (`org_kde_kwin_fake_input`) from a
small test client in the nested session, or test the gesture state machine
purely in unit tests and leave real key handling to manual checks. Do not use
`ydotool`/uinput: it injects into the live session.

## Steps for the user (once the sandbox and packages allow it)

Run outside the sandbox (or after option 1 above):

```
scripts/dev-session.sh start       # prints exports; state in $TMPDIR/vdw-dev-session
scripts/dev-session.sh selftest    # expect four PASS lines
scripts/dev-session.sh stop
```

`selftest` checks: `org.kde.KWin /Scripting` answers on the nested bus;
`org.kde.kglobalaccel` is on the nested bus (and who owns it); a Konsole
started with `run -- konsole --separate` shows up in
`workspace.windowList()` from a KWin script; `kdotool search --class konsole`
finds it through the nested bus. Paste the output into this file.

Build and test after installing `extra-cmake-modules` and `ninja`:

```
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
```
