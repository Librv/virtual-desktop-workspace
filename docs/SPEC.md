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
   captures the current desktop's windows into a new workspace.

## Opening a workspace

A workspace always opens on the **currently active** virtual desktop. No new
desktops are created.

- **Empty desktop:** the apps launch and are placed immediately.
- **Desktop already has windows:** nothing launches yet. A Plasma on-screen
  message appears, e.g. "Desktop not empty. Tap D again to set up 3 Konsoles
  here." Pressing the same shortcut again confirms, which with the modifiers
  still held is just tapping D again. Without a confirm, the request expires
  after 2 seconds (configurable) and the message fades.
  - After a **tap**, letting go of Ctrl or Meta cancels the request right
    away.
  - After a **radial menu pick**, the user has usually let go of every key,
    so letting go does not cancel; pressing Ctrl+Meta+D again within the
    timeout confirms.
- This applies to every way of opening a workspace: the main shortcut, a
  per-workspace shortcut, and the radial menu.
- On confirm, existing windows are left untouched; new windows go into their
  slots even if they overlap existing ones.

## The main shortcut: Ctrl+Meta+D

| Gesture | Result |
|---|---|
| Tap (press and release quickly) | Open the default workspace (with the confirm flow above if the desktop is busy) |
| Hold (still pressed after a short threshold, 250 ms, configurable) | Show the radial menu |
| Tap D again while a confirm message is showing | Confirm |

If no default workspace is set (or it was deleted), a tap shows a short
on-screen hint instead; holding still opens the menu.

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
