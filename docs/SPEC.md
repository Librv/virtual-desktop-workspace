# Product spec

What the app does, from the user's point of view. Implementation lives in
`ARCHITECTURE.md`; open questions and settled choices live in `DECISIONS.md`.

## Goal

Set up a predefined set of apps, placed at predefined positions, on the current
virtual desktop with one keyboard shortcut. It should feel like part of KDE
Plasma: shortcuts editable in System Settings, Plasma-styled popups, a System
Settings page for configuration.

Target: KDE Plasma 6 on Wayland, single monitor (multi-monitor is out of scope
for now).

## Workspaces

A **workspace** is a named, saved layout, for example "3 Konsoles":

- a name and an optional icon
- the System Settings page shows a pager-style preview of the placement: a
  mini screen with one rectangle per app, each in its own color, with its icon;
  the app's entry in the list has a border in the same color
- an optional shortcut of its own
- a list of apps, each with:
  - the app (its `.desktop` file id, e.g. `org.kde.konsole`)
  - optional extra command-line arguments
  - a position and size, stored as fractions of the screen area not covered by
    panels (so layouts survive resolution and panel changes)

One workspace is marked as the **default**.

Workspaces can be created two ways:

1. In the System Settings page, by adding apps and positions by hand.
2. "Save current desktop as workspace": the user arranges real windows, then
   captures the current desktop's windows into a new workspace. The windows
   of the program that asked for the capture (System Settings itself) are
   left out.

## Opening a workspace

A workspace always opens on the **currently active** virtual desktop. No new
desktops are created.

- **Empty desktop:** the apps launch and are placed immediately.
- **Desktop already has windows:** nothing launches yet. A toast appears
  centered at the top of the screen, e.g. "Desktop not empty. Keep holding
  Ctrl+Meta and press D to set up 3 Konsoles here." The only way to confirm
  is to keep Ctrl+Meta held and press D again, after a tap and after a radial
  menu pick alike. Letting go of Ctrl or Meta cancels right away and hides
  the toast; so does the 2 second timeout (configurable). If the modifiers
  are already up when the desktop turns out to be busy, the request is
  cancelled without a toast.
- This applies to every way of opening a workspace: the main shortcut, a
  per-workspace shortcut, and the radial menu.
- On confirm, existing windows are left untouched; new windows go into their
  slots even if they overlap existing ones.
- Where a workspace's own windows overlap, the order of its apps decides:
  the last app in the list is on top, the first at the bottom. The settings
  page can move apps up and down the list. Capture saves windows in this
  order (bottom first).

## The main shortcut: Ctrl+Meta+D

| Gesture | Result |
|---|---|
| Tap (press and release quickly) | Open the default workspace (with the confirm flow above if the desktop is busy) |
| Hold (still pressed after a short threshold, 250 ms, configurable) | Show the radial menu |
| Tap D again while a confirm message is showing | Confirm |

If no default workspace is set (or it was deleted), a tap shows a short
toast instead (it hides after 3 seconds); holding still opens the menu.

Per-workspace shortcuts only support tap (and the same confirm flow); they
never open the menu.

## Radial menu

Shown while the main shortcut is held.

- Appears centered on the mouse pointer, drawn over everything, styled with the
  Plasma theme.
- One slice per saved workspace, showing its icon and name.
- Moving the mouse highlights the slice in that direction; distance does not
  matter outside a small dead zone in the center.
- **Release** over a slice: open that workspace. On a busy desktop this goes
  through the same confirm flow as a tap: the message appears and tapping D
  again (while holding Ctrl+Meta) within the timeout confirms.
- **Release** in the center dead zone: cancel. There is no Esc: with Ctrl+Meta
  held it would trigger KWin's Kill Window shortcut.
- No saved workspaces: show a single hint ("No workspaces yet") instead.

## Out of scope for now

- Multiple monitors
- Restoring app state (open files, tabs, terminal directories)
- Moving or closing windows that were already on the desktop
- Activities
