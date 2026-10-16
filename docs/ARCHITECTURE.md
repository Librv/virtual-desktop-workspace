# Architecture

Target design. Parts marked *(spike)* depend on a spike in `docs/tasks/` and
may change once its findings are written up in `docs/spikes/`.

## Why not keep the bash script

`setup-workspace.sh` (bash + `kdotool`) proved the idea but cannot grow into
the spec: it cannot tell a tap from a hold, cannot draw a menu, registers its
shortcut through a `.desktop` file that `kglobalaccel` caches unreliably, and
polls for new windows every 50 ms. It stays as the working version until the
packaging task (T7) replaces it.

## Components

```
                 Ctrl+Meta+D press/release
  kglobalaccel  ───────────────────────────►  vdwd (daemon, C++/Qt6/KF6)
                                               │  gesture state machine
                                               │  config (KConfig)
   Plasma OSD   ◄── "desktop not empty" ───────┤
                                               │
   radial menu  ◄── show / hide / selection ───┤  (QML, LayerShellQt overlay)
                                               │
   KWin script  ◄── "place app X at rect R" ───┤  launches apps via
   (in KWin)    ──► window opened / placed ───►│  KIO::ApplicationLauncherJob
                                               │
   vdwctl, KCM  ◄────────── D-Bus API ─────────┘
```

### vdwd: the daemon

C++ / Qt 6 / KDE Frameworks 6, started at login via XDG autostart (or a
systemd user unit).

- **Shortcuts:** one `QAction` per shortcut registered with `KGlobalAccel`, so
  they show up and are editable in System Settings, Shortcuts.
  `KGlobalAccel::globalShortcutActiveChanged(QAction*, bool)` gives press and
  release, which the gesture state machine needs.
- **Gesture state machine:** pure logic, no Qt GUI, with an injectable clock so
  it is unit-testable. States: idle, pressed (waiting for hold threshold),
  menu open, awaiting confirm. Inputs: press, release, timer, menu selection,
  emptiness result. Outputs: open workspace, show menu, hide menu, show OSD.
- **Config:** KConfig file `~/.config/vdwrc` (exact schema decided in T1).
- **Launching:** `KIO::ApplicationLauncherJob` from a `.desktop` id plus args.
- **OSD:** Plasma's on-screen display over D-Bus (`org.kde.plasmashell`,
  `/org/kde/osdService`, `showText`).
- **D-Bus API** (`org.kde.vdw` or similar, decided in T1): list workspaces,
  open workspace, capture current desktop, reload config. Debug builds also
  expose methods that inject gesture events, so tests can drive the state
  machine without real key presses.

### KWin script

JavaScript KWin script shipped with the daemon and loaded through KWin's
`org.kde.kwin.Scripting` D-Bus interface. It handles everything that needs to
run inside the compositor:

- report whether the current desktop has normal windows (the check the bash
  script already does)
- report the usable screen area (`workspace.clientArea(KWin.PlacementArea, ...)`)
- on `workspace.windowAdded`, match new windows against pending placements and
  set their geometry, then report back with `callDBus`
- list the current desktop's windows with geometry and app id, for "Save
  current desktop as workspace"

This replaces polling with events. *(spike S3)*

### Matching windows to launches

Matching by process id breaks for apps that reuse one process (Dolphin,
Firefox, Konsole when "single process" is on). Plan: each launch adds a pending
placement keyed by the app id (`desktopFileName` / resource class); new windows
claim pending placements in launch order, with a timeout. *(spike S4)*

### Radial menu

QML scene in a full-screen transparent LayerShellQt surface on the overlay
layer, owned by the daemon. Plasma theme colors via Kirigami. Selection is
computed from the pointer angle relative to the center (pure function,
unit-tested). Open questions *(spike S2)*: does the surface get pointer motion
without taking keyboard focus, how to get the initial pointer position on
Wayland, and does the shortcut release still reach `kglobalaccel` while the
overlay is up.

### vdwctl

Small command-line tool that calls the D-Bus API. Used for scripting, for
tests, and as a fallback way to bind workspaces.

### KCM (System Settings page)

QML KCM to list, create, edit and delete workspaces, set the default, edit
slots, and trigger "Save current desktop as workspace". Shortcuts link to the
standard Shortcuts page rather than reimplementing a key editor.

## Testing

- **Unit tests (Qt Test):** gesture state machine, config parsing, slice
  selection math, fraction-to-rectangle layout math.
- **Integration tests:** a nested, headless KWin (`kwin_wayland --virtual` in
  its own D-Bus session) so tests launch and place real windows without
  touching the user's desktop. Set up in T0b.
- **Manual:** the radial menu feel and real key handling on the live session,
  done by the user.
