# T3b findings: shortcuts, gesture machine, OSD and menu wired into vdwd

Date: 2026-10-17. KWin 6.7.5, KF6 6.31, Qt 6.12. Everything below ran in the
nested session (`VDW_DEV_DIR=/run/user/1000/vdw-t3b scripts/dev-session.sh`),
never on the live session.

## What was built

- `src/daemon/shortcuts.{h,cpp}`: component `vdw` ("Virtual Desktop
  Workspace"), action `main` (default Ctrl+Meta+D) and `open-<id>` per
  workspace. `setAutoRepeat(false)`, default with `NoAutoloading`, current
  with `Autoloading`. Input only from `globalShortcutActiveChanged`.
  `KModifierKeyInfo` feeds `modifiersHeld`, derived from the current
  `QKeySequence` of `main` (re-derived on `globalShortcutChanged`).
- `src/lib/shortcutkeys.{h,cpp}`: the pure helpers (modifier keys of a
  sequence, key text of a sequence). `vdwcore` now links `Qt6::Gui` for
  `QKeySequence`.
- `src/daemon/gesturecontroller.{h,cpp}`: owns the `GestureMachine`, runs its
  commands (timers, open, menu, OSD) and feeds results back.
- `src/daemon/kwinscripts.*`, `kwin/cursor.js`: `queryCursor()` loads a
  one-shot script that reports `workspace.cursorPos` through
  `ScriptApi::CursorPos`.
- `Manager::open(id, force, reply)` is the internal open (the D-Bus method
  uses it), `Manager::config()` and `configReloaded()` exist for the
  controller and the shortcuts. `/Debug` is now `Press(s)`, `Release(s)`,
  `ModifiersHeld(b)` (ARCHITECTURE.md table updated).

## Findings

- **Renaming a shortcut needs a direct call.** `KGlobalAccel` only sends the
  friendly name when an action is first registered. After a workspace is
  renamed, re-calling `setShortcut` did not change what kglobalacceld shows
  in System Settings. `org.kde.KGlobalAccel.doRegister(as)` with the same
  action id and the new friendly name does, and keeps the keys, so
  `Shortcuts::sync` calls it. Checked in the nested session through
  `Component.allShortcutInfos` (rename, add and delete via `ReloadConfig`).
- **Deleted workspaces** lose their global shortcut on `ReloadConfig`
  (`removeAllShortcuts` for that action only); the others and `main` keep
  theirs. On startup `Shortcuts::sync` lists the actions of component
  `vdw` and unregisters every `open-<id>` whose id is not in the config
  (`KGlobalAccel.unregister`; never `main`). Checked in the nested session.
- **Names with "&"**: KGlobalAccel removes every "&" of the action text when it
  registers, so "R&D" showed as "RD" on a fresh start. The visible name is now
  also sent with `doRegister` after the first registration, the same call a
  rename uses, so both paths give "R&D".
- **No default workspace**: a tap shows the OSD "No default workspace. Hold the
  shortcut for the menu, or pick one in System Settings." (logged only in the
  nested session).
- **Confirm key** comes from the shortcut that issued `Open(id, false)`
  (recorded per workspace id), not the last pressed one.
- **Release before the menu is up** cancels: the controller bumps a
  generation on `ShowMenu` and `HideMenu`, so a cursor answer that arrives
  after the release never shows the menu.
- **The confirm timeout is 2 s.** An integration test that waits exactly
  2 s between a busy tap and the confirming tap flakes (the confirm has
  expired). Tests wait 800 ms.
- **KWin only learns about an injector's `.desktop` file** if it is in the
  nested XDG_DATA_HOME; the integration test writes `vdw-s1-inject.desktop`
  and `vdw-s2-fakeinput.desktop` itself (only when the data home path
  contains `/vdw`) and it worked without restarting the session or running
  `kbuildsycoca6`.
- Qt logging: when stderr is not a terminal Qt logs to the journal, so the
  nested vdwd printed nothing; set `QT_ASSUME_STDERR_HAS_CONSOLE=1` to see
  the `vdw.daemon` lines.
- There is no plasmashell in the nested session, so `ShowConfirmOsd` only
  logs "cannot show the OSD". The real OSD is a live-session check.

## Verification (nested session)

Unit tests: `ctest --preset dev` all pass, including `shortcutkeystest`
(modifier set and key text from a `QKeySequence`).

`integrationtest` (run with `scripts/dev-session.sh run -- build/dev/bin/integrationtest`):

| Case | Result |
|---|---|
| tap of `main` on an empty desktop opens the default workspace | pass |
| tap on a busy desktop opens nothing, second press with modifiers held confirms (force) | pass |
| `ModifiersHeld(false)` between the two taps cancels the confirm | pass |
| hold of `main` shows the overlay (a `vdwd` window in KWin's stacking order), release over the dead zone cancels | pass |
| workspace shortcut opens its own workspace, no overlay even when held 700 ms | pass |
| real keys: Ctrl+Meta+D tap through `s1inject` opens the default workspace | pass |
| best effort, real keys: hold Ctrl+Meta+D, overlay appears, pointer moved up onto the first slice with `s2-fakeinput`, release, workspace `one` opens full screen and the overlay is gone | pass |

The two real-key cases skip when `spikes/S1/build/s1inject` or
`spikes/S2/build/s2-fakeinput` are not built (`cmake -S spikes/S1 -B
spikes/S1/build -G Ninja && cmake --build spikes/S1/build`, same for S2).

## Steps for the user on the live session

vdwd registers Ctrl+Meta+D, which the legacy `net.local.setup-workspace.sh.desktop`
binding also uses. Remove that binding first (System Settings, Shortcuts,
find the "setup-workspace" entry, clear its shortcut) or run vdwd with a
different key (step 3). The checks below open real windows on the current
desktop: use an empty one.

1. Build: `cmake --preset dev && cmake --build --preset dev`.
2. Write a small test config, for example `/tmp/vdw-t3b.rc`:

   ```
   [General]
   DefaultWorkspace=a

   [Workspace][a]
   Name=Alpha
   Order=0
   Icon=folder

   [Workspace][a][Slot][0]
   App=org.kde.konsole
   Geometry=0,0,0.5,1

   [Workspace][b]
   Name=Beta
   Order=1

   [Workspace][b][Slot][0]
   App=org.kde.konsole
   Geometry=0.5,0,0.5,1
   ```

3. Start the daemon in a terminal: `build/dev/bin/vdwd --config /tmp/vdw-t3b.rc`.
   To avoid the legacy key, open System Settings, Shortcuts, "Virtual
   Desktop Workspace", and set another chord for "Open default workspace,
   hold for the workspace menu" (the entry appears after vdwd has started;
   the change takes effect at once and survives restarts).
4. Tap Ctrl+Meta+D on an empty desktop: Konsole opens on the left half.
5. Tap it again while the desktop is not empty: nothing opens and a message
   "Desktop not empty. Press D again to set up Alpha here." appears (the
   Plasma OSD, with the folder icon). Tap D again while still holding
   Ctrl+Meta within 2 s: a second Konsole opens. Repeat, but let go of
   Ctrl and Meta before the second tap: nothing opens.
6. Hold Ctrl+Meta+D for more than 250 ms: a radial menu with Alpha and Beta
   appears around the pointer. Move onto a slice and release D (or any of
   the keys): that workspace opens. Release over the center: nothing opens.
   Check that the menu follows the pointer position at the time of the
   hold, that it looks right in your light and dark theme, and that Esc
   needs no handling (the menu takes no keyboard focus).
7. In System Settings, Shortcuts, "Virtual Desktop Workspace" lists
   "Open workspace: Alpha" and "Open workspace: Beta" with no key. Assign
   one, press it: only that workspace opens, never the menu.
8. Edit `/tmp/vdw-t3b.rc` (rename Beta, delete it, add a workspace), run
   `vdwctl reload`, and look at the Shortcuts page again: names follow, a
   deleted workspace's entry disappears, the others keep their keys.
9. Stop vdwd with Ctrl+C. The shortcut entries stay (by design); to clean
   up, remove the "Virtual Desktop Workspace" component in System Settings.

Things only the live session can show: the real Plasma OSD text and icon,
real keyboard auto-repeat and layout behavior (S1 "Not verified"), a
shortcut conflict with the legacy binding, and whether the overlay is above
fullscreen windows.

## Open issues

- `ShowMenu` has no fallback for a KWin query that times out other than
  using the primary screen center (logged).
