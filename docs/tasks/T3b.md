# T3b: Wire shortcuts, gesture machine, OSD and menu into vdwd

**Depends on:** T2, T3a, T4a. **Owns:** `src/daemon/`, `autotests/`. May
add sources to `src/lib/` only if a pure helper needs a home there. Do not
change `src/gesture/` or `src/menu/` behavior; report back if their API is
not enough.

Behavior: `docs/SPEC.md`. Design: `docs/ARCHITECTURE.md` (vdwd, D-Bus API,
Radial menu), DECISIONS 5, 7 to 11, 16, 17, `docs/spikes/S1.md`
("Recommendation for T3"), `docs/spikes/S2.md`.

## Design (fixed)

- **`src/daemon/shortcuts.{h,cpp}`:** KGlobalAccel actions under component
  `vdw` (display name "Virtual Desktop Workspace"): action `main`
  ("Open default workspace, hold for the workspace menu", default
  Ctrl+Meta+D) and one action `open-<id>` per workspace ("Open workspace:
  <Name>", no default key). `setAutoRepeat(false)`, default registered with
  `NoAutoloading`, current with `Autoloading` (S1). Input only from
  `globalShortcutActiveChanged`. On `ReloadConfig`: add actions for new
  workspaces, update names, and remove the global shortcut of workspaces
  that were deleted from the config (only those; never `removeAllShortcuts`
  for actions that still exist, and nothing on exit).
- **Modifiers:** `KModifierKeyInfo`; "held" means every modifier of the main
  shortcut's current `QKeySequence` is down (derived from the sequence, not
  hard coded). Feed changes into the machine as `modifiersHeld`.
- **`src/daemon/gesturecontroller.{h,cpp}`:** owns the `GestureMachine`
  (config from `[General]`: default workspace, hold threshold, confirm
  timeout; refreshed on reload) and executes its commands:
  - `StartTimer(token, ms)`: a single-shot `QTimer`, then
    `timerFired(token)`.
  - `Open(id, force)`: call the Manager's internal open with a callback
    (make it reachable from the controller; keep the D-Bus method for
    vdwctl). Reply `busy` gives `openResult(id, true)`; anything else gives
    `openResult(id, false)` (log errors).
  - `ShowMenu`: run a one-shot `cursor.js` query (`workspace.cursorPos`,
    reported through `CursorPos`), then `RadialMenu::show(center, items)`
    with the workspaces in config order (name, icon). If the release
    arrives before the menu is up, the selection is empty (cancel).
  - After feeding a `release`, if the machine is in `AwaitingMenuResult`,
    call `menuResult(RadialMenu::selectedId())`.
  - `HideMenu`: `RadialMenu::hide()`.
  - `ShowConfirmOsd(id)`: `org.kde.plasmashell` `/org/kde/osdService`
    `org.kde.osdService.showText(icon, text)` with the workspace icon and
    "Desktop not empty. Press <key> again to set up <Name> here.", where
    `<key>` is the non-modifier key of the confirming shortcut (e.g. "D").
    Fire and forget; log a failure (there is no plasmashell in the nested
    session).
- **Debug API change** (update `ARCHITECTURE.md` D-Bus table accordingly in
  the same change set, nothing else in the docs): `/Debug` methods become
  `Press(s shortcut)`, `Release(s shortcut)`, `ModifiersHeld(b)`, where
  `shortcut` is `main` or a workspace id. They feed the controller through
  the same path as real input.
- The emptiness check already ignores vdwd's own windows (T2), which covers
  the menu overlay.

## Acceptance

- Unit tests where logic is pure (e.g. deriving the modifier set and the
  confirm key from a `QKeySequence`).
- Nested integration tests driven by `/Debug`:
  - tap of `main` on an empty desktop opens the default workspace;
  - tap on a busy desktop opens nothing, then a second press of `main`
    with modifiers held confirms (force) and the windows appear;
  - tap path: `ModifiersHeld(false)` between the two presses cancels;
  - a workspace shortcut opens its own workspace and never the menu.
- Nested integration test with real key events (`spikes/S1/s1inject`,
  authorized as in S1): a Ctrl+Meta+D tap opens the default workspace.
- Best effort, report the result either way: hold Ctrl+Meta+D with real
  keys, check the overlay surface exists (KWin script listing), move the
  pointer with the S2 fake input tool onto a slice, release, and check that
  workspace opened.
- Live-session steps for the user written to `docs/spikes/T3b.md` (the
  user's legacy `net.local.setup-workspace.sh.desktop` binding also uses
  Meta+Ctrl+D; the steps must tell them to remove it first or test with a
  different key).
