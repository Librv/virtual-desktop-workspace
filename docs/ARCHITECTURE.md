# Architecture

Target design, updated with the findings of spikes S1 to S4 (details and
measurements in `docs/spikes/`).

## Why not keep the bash script

`setup-workspace.sh` (bash + `kdotool`) proved the idea but cannot grow into
the spec: it cannot tell a tap from a hold, cannot draw a menu, registers its
shortcut through a `.desktop` file that `kglobalaccel` caches unreliably, and
polls for new windows every 50 ms. It stays as the working version until the
packaging task (T7) replaces it.

## Components

```
                 Ctrl+Meta+D press/release (D-Bus)
  kglobalaccel  ───────────────────────────►  vdwd (daemon, QtCore/QtDBus only)
                                               │  gesture state machine
                                               │  config (KConfig)
   vdw-ui       ◄── stdin/stdout lines ───────►│  starts one helper per
   (per gesture: radial menu, toast,           │  gesture or launch
    Ctrl/Meta held, KIO launching)             │
                                               │
   KWin script  ──► ClaimWindow / results ────►│
   (in KWin)    ◄── placement rect ────────────┤
                                               │
   vdwctl, KCM  ◄────────── D-Bus API ─────────┘
```

### vdwd: the daemon

C++ / Qt 6 / KDE Frameworks 6, started at login via XDG autostart (or a
systemd user unit), plus a D-Bus `.service` file so a `callDBus` from the KWin
script can start it.

- **No Qt GUI (S5):** vdwd is a `QCoreApplication` and links only Qt
  Core/DBus and KF6 ConfigCore, CoreAddons and Service (KF6 GlobalAccel,
  DBusAddons, GuiAddons and KIOGui all pull in libQt6Gui). Everything that
  needs a GUI or Wayland runs in `vdw-ui` (below). It registers its D-Bus
  name with plain `registerService` and exits if the name is taken.
- **Shortcuts (S1, S5):** one component `vdw` in kglobalaccel, one action per
  shortcut (main shortcut plus one per workspace), registered over raw D-Bus
  (`org.kde.kglobalaccel` `/kglobalaccel`): `doRegister`, then
  `setShortcutKeys` with IsDefault|NoAutoloading (12) for the default and
  SetPresent (2) to activate, so edits made in System Settings survive
  restarts. Key sequences are `a(ai)` of Qt key|modifier ints. vdwd caches
  each action's keys from the replies and from `yourShortcutsChanged`, and
  re-registers when kglobalaccel reappears. Never remove the user's bindings
  on exit.
- **Gesture input (S1, S5):** `globalShortcutPressed` / `globalShortcutReleased`
  on `/component/vdw`. The release fires when any key of the chord goes up.
  Ctrl/Meta held state comes from `KModifierKeyInfo` in `vdw-ui` (Wayland
  keystate protocol, reported 150 ms after start, then on every change); vdwd
  assumes held at a main press. The modifier set is derived from the
  configured key sequence, not hard coded.
  Never use kglobalaccel's `invokeShortcut` (press without release, swallows
  the next real press).
- **Gesture state machine (S1):** pure logic, no Qt GUI, injectable clock,
  unit-tested. Inputs: press, release, modifiersHeld(bool), timer, menu
  selection, emptiness result. Outputs: open workspace, show menu, hide menu,
  show OSD. Rules are in `SPEC.md`; in short:
  - idle: press starts the hold timer (250 ms).
  - pressed: release before the timer is a tap; timer firing opens the menu.
  - menu: release selects (dead zone = cancel).
  - awaiting confirm (2 s timer): a new press of the same shortcut confirms.
    In the tap path, modifiers going up also cancels immediately; in the menu
    path only the timeout cancels (the user has usually let go of every key
    when picking).
  - Must tolerate release without press, press without release, and stale
    timers (generation counters).
- **Launching (S4, S5):** done by `vdw-ui` with `KIO::ApplicationLauncherJob`
  and a copy of the app's `KService` whose `Exec` gets the slot's extra args
  appended (KIO keeps the systemd scope, activation token and field codes).
  The pid is read in the job's `result` handler and sent back to vdwd.
- **Toast:** our own layer-shell toast at the top center (decision 27), shown
  by `vdw-ui`.

### vdw-ui: the GUI helper (S5)

Short-lived `QGuiApplication` with the software scene graph renderer. vdwd
starts it next to its own binary on a main-shortcut press (about 65 ms, so it
is up before the 250 ms hold threshold) or when it needs a toast or a launch,
and sends `quit` once the gesture is idle, no toast is visible and no launch
is pending (killed after 2 s if it does not exit). It exits on stdin EOF, so
it never outlives vdwd. The line protocol is in `docs/spikes/S5.md`.

### Config

KConfig file `~/.config/vdwrc`. Shortcut keys are not stored here:
KGlobalAccel owns them (component `vdw`, action `open-<id>` per workspace and
`main` for the main shortcut).

```ini
[General]
DefaultWorkspace=3-konsoles
HoldThresholdMs=250
ConfirmTimeoutMs=2000
PlacementTimeoutMs=10000

[Workspace][3-konsoles]
Name=3 Konsoles
Icon=utilities-terminal
Order=0

[Workspace][3-konsoles][Slot][0]
App=org.kde.konsole
Args=
Geometry=0.5,0,0.5,0.5
```

- A workspace id is a stable slug (`[a-z0-9-]+`), used in D-Bus calls and
  shortcut action names. `Name` is what the user sees.
- `App` is a `.desktop` id. `Args` is one string split with
  `KShell::splitArgs`.
- `Geometry` is `x,y,width,height` as fractions (0 to 1) of the usable area
  (`clientArea(PlacementArea, ...)`, so panels are excluded).
- Slots are numbered from 0; workspaces are sorted by `Order`, then `Name`.
- Validation (settled in T1): a bad workspace id, slot name, slot geometry or
  missing `App` skips that workspace or slot with a warning; unknown keys only
  warn. Geometry values must each be in 0..1 with width and height > 0
  (x + width may exceed 1). A workspace without valid slots is kept, a
  missing `Name` falls back to the id, an unknown `DefaultWorkspace` and bad
  `[General]` values fall back to defaults with a warning. Sorting is
  `Order`, then `Name` case-insensitively, then id.
- In `ListWorkspaces`, a slot is `{app, args, geometry: {x, y, width,
  height}}`.

### D-Bus API

Service `io.github.librv.vdw`. All structured data is passed as JSON strings
(KWin's `callDBus` handles strings and numbers well, complex types badly; JS
numbers arrive as int32, so integer parameters are `i`).

| Object / interface | Method | Purpose |
|---|---|---|
| `/Manager`, `io.github.librv.vdw.Manager` | `ListWorkspaces() -> s` | JSON array of `{id, name, icon, isDefault, slots}` |
| | `OpenWorkspace(s id, b force) -> s` | `"opened"`, `"busy"` (desktop not empty and not forced), or `"error: ..."` |
| | `CaptureCurrentDesktop(s name) -> s` | new workspace id, or `"error: ..."` (T6) |
| | `ReloadConfig()` | re-read `vdwrc`, re-register shortcuts |
| `/Script`, `io.github.librv.vdw.Script` | `ClaimWindow(s windowJson) -> s` | called by the KWin script on `windowAdded`; returns `{rect, group, order}` JSON or `""` |
| | `QueryResult(s requestId, s json)` | answers of one-shot query scripts |
| | `CursorPos(i x, i y)` | pointer position for the radial menu center |
| `/Debug`, `io.github.librv.vdw.Debug` | `Press(s shortcut)`, `Release(s shortcut)`, `ModifiersHeld(b)` | only when started with `--debug`: feed the state machine through the same path as real input; `shortcut` is `main` or a workspace id |

### KWin script (S3)

- **Long-lived placer script**, loaded at daemon startup through
  `org.kde.kwin.Scripting` (`isScriptLoaded` then `unloadScript` then
  `loadScript` then `Scripting.start()`, because loading a name that is still
  loaded fails). Never run a script through `/Scripting/Script<id>`: KWin
  derives the id from the number of loaded scripts, so after an unload a new
  script can get the id of another live one (T2). `start()` runs every
  loaded script that is not running yet. The placer is reloaded when
  `org.kde.KWin` reappears on the bus. On `workspace.windowAdded` it calls `ClaimWindow` with the window's
  pid, `desktopFileName`, `resourceClass` and `internalId`. If a match comes
  back (`{rect, group, order}`: the rect, one id per open, the slot index) it
  sets `frameGeometry` (it sticks; no re-apply needed), then raises the live
  windows of that group in slot order with `workspace.raiseWindow`, so the
  last slot ends on top whatever order the apps map in. Other windows and
  focus are not touched.
- **One-shot query scripts** for the emptiness check, the usable area, the
  window list (T6) and the cursor position: loaded, run, they answer through
  `QueryResult`/`CursorPos`, then unloaded.
- App windows are `normalWindow && minimizable` (plain `normalWindow` also
  matches layer-shell surfaces, including our own radial menu). The emptiness
  check additionally ignores vdwd's own windows.
- Window lists come from `workspace.windowList()`; `workspace.stackingOrder`
  still contains windows that were just closed, so it is only used to sort.
- No shortcut "doorbell" (`registerShortcut` persists in the user's shortcut
  config) and no script config (`readConfig` does not see external writes).

### Matching windows to launches (S4)

The daemon owns the pending placements and decides; the script only reports.
Matching is a pure, unit-tested class (launches and observed windows in,
assignments out):

1. pid match first (job pid equals window pid): exact for Konsole (normal
   mode), Dolphin and cold-started Kate.
2. Otherwise the oldest pending entry with the same `desktopFileName`
   (`resourceClass` if empty), for apps that share one process.
3. Entries expire after `PlacementTimeoutMs` (10 s). A launch that opens no
   window is normal (e.g. Kate handing off to a running instance).

Windows that match nothing are left alone. Activation tokens are not visible
to KWin scripts, so they are not used. Captions are not stable; never match
on them.

### Radial menu (S2)

Shown by `vdw-ui`. QML scene in a full-screen transparent LayerShellQt surface on the overlay
layer, all four anchors, exclusive zone -1, keyboard interactivity none (the
active window keeps focus), Kirigami theme colors. Created on demand (about
65 ms to first frame in the nested session; keep it alive only if it feels
slow). The center comes from KWin's `workspace.cursorPos` via a one-shot
script (`CursorPos`), because the surface gets no `enter` event for a resting
pointer; the first `enter` is only a fallback. The selected slice is computed
from the last pointer position with `RadialMath::sliceAt` (pure, tested;
taken from `spikes/S2/`) when the gesture state machine reports the release.
There is no Esc: holding Ctrl+Meta turns Esc into KWin's Kill Window
shortcut. Cancel is releasing in the dead zone.

### vdwctl

Small command-line tool over the Manager interface (`list`, `open <id>
[--force]`, `capture <name>`, `reload`). Used for scripting and tests.

### KCM (System Settings page)

QML KCM to list, create, edit and delete workspaces, set the default, edit
slots, and trigger "Save current desktop as workspace". Shortcuts link to the
standard Shortcuts page rather than reimplementing a key editor.

## Testing

- **Unit tests (Qt Test):** gesture state machine, config parsing, window
  matching, slice selection math, fraction-to-rectangle layout math.
- **Integration tests:** the nested, headless KWin from
  `scripts/dev-session.sh` (own bus, HOME and XDG dirs). Run them as
  `scripts/dev-session.sh run -- ctest ...`; they must skip, not fail, when no
  nested session is reachable. The nested session can do more than first
  assumed (S1, S2):
  - key and pointer events through `org_kde_kwin_fake_input`, from a helper
    authorized by a `.desktop` file with
    `X-KDE-Wayland-Interfaces=org_kde_kwin_fake_input` in the nested
    `XDG_DATA_HOME` (`spikes/S1/s1inject`, `spikes/S2/tools/fakeinput.c`);
  - screenshots through KWin's ScreenShot2 (`.desktop` with
    `X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2`, then
    `kbuildsycoca6`; `spikes/S2/tools/shot.cpp`).
  So real shortcut press/release, the overlay and placement are all testable
  there.
- **Manual:** feel (gesture timing, menu look) and anything involving real
  hardware, done by the user on the live session.
