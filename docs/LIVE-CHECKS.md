# Live-session checklist

Steps for the user, on the real Plasma session, with the `vdw` package
installed and the legacy `setup-workspace.sh` shortcut removed (see "Migrating"
in `README.md`). They open real windows: do them on an empty desktop
unless a step says otherwise. These merge the user steps of `spikes/S1.md`,
`spikes/S2.md` and `spikes/T3b.md`; the nested session could not show them.

## 0. Preparation

1. Log out and in, or run `vdwctl list` (D-Bus activation starts vdwd with
   the session environment; do not run `vdwd` from a terminal, its apps would
   inherit that terminal's environment). Check: `vdwctl list` prints JSON.
   Expected: on a first run without `~/.config/vdwrc`, one workspace
   "3 Konsoles" marked `isDefault`.
2. Open System Settings, Shortcuts, search "Virtual Desktop Workspace".
   Expected: an entry with "Main gesture" bound to Ctrl+Meta+D.

## 1. Tap

3. On an empty desktop, click a blank spot, then tap Ctrl+Meta+D quickly.
   Expected: three Konsoles open at top-right, bottom-left and bottom-right
   quadrants, not under the panel; top-left stays empty.
4. Close the three Konsoles again.
5. Press and release only Meta (no D). Expected: the launcher opens as usual.

## 2. Hold and radial menu

6. Add a second workspace in the settings page first (step 19) so the menu has
   two slices. Click into some window, then hold Ctrl+Meta+D for more than
   250 ms. Expected: the radial menu appears centered on the pointer, above
   panels and windows, in the Plasma theme; the window below keeps focus.
7. Move the pointer without clicking. Expected: the slice in the pointer's
   direction highlights; the center shows a cancel icon.
8. Release over a slice. Expected: that workspace opens (empty desktop), the
   menu disappears, no stray "d" reaches the window below.
9. Hold again and release in the center. Expected: nothing opens.
10. Hold the shortcut over a panel, a fullscreen video or game window, and a
    window using pointer lock. Expected: the menu covers them; report any it
    does not cover or breaks.
11. Hold, release only Ctrl, then Meta and D. Expected: no crash, the menu
    closes, and no workspace opens unless the pointer was on a slice at the
    moment of release.

## 3. Confirm on a busy desktop

12. With windows on the desktop, tap Ctrl+Meta+D and keep Ctrl+Meta held.
    Expected: nothing launches; a toast centered at the top of the screen
    reads "Desktop not empty. Keep holding Ctrl+Meta and press D to set up
    <name> here."
13. Press D again within 2 seconds while still holding Ctrl+Meta. Expected: the
    toast goes away and the workspace opens on top of the existing windows
    (which stay untouched).
14. Tap, then let go of Ctrl or Meta. Expected: the toast disappears right
    away and nothing launches. Pressing D afterwards is an ordinary tap. A very
    short tap that lets go of Ctrl+Meta together with D shows no toast at all.
15. Tap, keep Ctrl+Meta held and do nothing for 3 seconds. Expected: the toast
    disappears after the 2 second timeout, nothing launches.
16. Radial menu path: on a busy desktop hold, release D over a slice while
    still holding Ctrl+Meta. Expected: the toast appears; pressing D again
    within 2 seconds opens the workspace. Repeat and let go of Ctrl or Meta
    instead. Expected: the toast disappears and nothing launches.

## 4. Workspace shortcut

17. In System Settings, Virtual Desktop Workspaces, give a workspace its own
    shortcut ("Configure shortcuts..." or the Shortcuts page). Tap it on an
    empty desktop. Expected: that workspace opens; holding it never shows the
    menu. On a busy desktop it asks to confirm like step 12, but only the 2 second
    timeout cancels (letting go of the modifiers does not).

## 5. Capture

18. Open two or three windows and arrange them. Open the settings page and click
    "Save current desktop as workspace" (or run `vdwctl capture "Mine"`).
    Expected: a new workspace "Mine" with one slot per window, appears in the
    list without restarting. Close the windows, open "Mine" from the menu.
    Expected: the windows come back at roughly the same places.

## 6. Settings page

19. Add, rename, reorder and delete a workspace, change its icon and the
    default, and click Apply. Expected: `vdwctl list` reflects it, the radial
    menu shows the new order and icons, and a deleted default makes a tap show
    the "no default" hint.
20. Edit a slot's app and position. Expected: opening the workspace follows the
    new values. Invalid input is shown as a problem and cannot be saved.

## 7. Shortcuts in System Settings

21. In System Settings, Shortcuts, rebind "Main gesture" to another chord
    (for example Ctrl+Meta+F9). Expected: the new chord taps and holds as
    before, the old one does nothing, and the modifier check follows the new
    chord (the confirm still needs the new modifiers).
22. Rebind it back to Ctrl+Meta+D.
23. Keyboard layout check (if you use a layout where D is not in the US place):
    the chord still works. If Ctrl+Meta+D is taken by another app, expect the
    usual conflict dialog.

## 8. Theme

24. Switch System Settings, Colors between Breeze Light and Breeze Dark (and an
    accent color), then hold the shortcut. Expected: text, highlight and the
    cancel icon stay legible; if the colors only change after a restart, run
    `pkill vdwd` (the D-Bus service restarts it on the next tap) or log out and
    in, and report it.

## 9. Lifecycle

25. Run `pkill vdwd`, then tap Ctrl+Meta+D. Expected: the D-Bus service starts
    vdwd again; the tap may be lost, the next one works. Run `vdwctl list` to
    confirm.
26. Log out and in. Expected: vdwd is running without any manual step.
27. `sudo pacman -R vdw`. Expected: the binaries, the settings page and the
    autostart entry are gone; Ctrl+Meta+D is free.
