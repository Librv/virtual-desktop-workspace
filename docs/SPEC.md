# Product spec

What the app does, from the user's point of view. Implementation lives in
`ARCHITECTURE.md`; open questions and settled choices live in `DECISIONS.md`.

## Goal

Set up a predefined set of apps, placed at predefined positions, on the current
desktop with one keyboard shortcut. It should feel like part of KDE
Plasma: shortcuts editable in System Settings, Plasma-styled popups, a System
Settings page for configuration.

Target: KDE Plasma 6 on Wayland, single monitor (multi-monitor is out of scope
for now).

## Workspaces

A **workspace** is a named, saved set of apps with their positions, for
example "3 Konsoles":

- a name and an optional icon
- the System Settings page shows a pager-style preview of the placement: a
  mini screen with one rectangle per app, each in its own color, with its icon;
  the app's entry in the list has a border in the same color
- an optional shortcut of its own
- a list of apps, each with:
  - the app (its `.desktop` file id, e.g. `org.kde.konsole`)
  - optional extra command-line arguments
  - for a browser (only Helium now): an optional browser profile. The browser
    opens in that profile. Without a profile, it opens its last-used profile
  - a position and size, stored as fractions of the screen area not covered by
    panels (so positions survive resolution and panel changes)

One workspace is marked as the **default**.

Workspaces can be created two ways:

1. In the System Settings page, by adding apps and positions by hand.
2. "Save current desktop as workspace": the user arranges real windows, then
   captures the current desktop's windows into a new workspace. The windows
   of the program that asked for the capture (System Settings itself) are
   left out.

## Opening a workspace

A workspace always opens on the **currently active** desktop. No new
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
- The toast and the confirm apply to every way of opening a workspace: the
  main shortcut, a per-workspace shortcut, and the radial menu. After a
  per-workspace shortcut, a new press of that shortcut confirms, and only the
  timeout cancels.
- On confirm, existing windows are left untouched; new windows go into their
  slots even if they overlap existing ones.
- Where a workspace's own windows overlap, the order of its apps decides:
  the last app in the list is on top, the first at the bottom. The settings
  page can move apps up and down the list. Capture saves windows in this
  order (bottom first).
- Some apps show a startup window first (the Discord updater, the Steam
  loading window). The first window of an app gets the slot. If that window
  closes and the main window of the app opens within 1 second of the close,
  before or after it, the main window takes the slot. This applies in the first
  5 minutes after the app got its slot. If both windows stay open, the second
  window gets no slot.
- A window that has a fixed size is never moved or resized. The startup
  windows of Discord and Steam have a fixed size, so they stay where the app
  puts them. The main window of the app takes the slot. An app whose only
  window has a fixed size is not placed.
- While a fixed-size window of a launch is open, the launch counts as busy
  for up to 5 minutes after the launch. The desktop then asks for
  confirmation, as with other busy desktops.
- Apps launch at the same time, with one exception: a browser gives a new
  window to its running process. When a workspace has two or more slots of
  one browser, vdwd launches those slots one at a time, in slot order. The
  next browser slot starts when the window of the previous slot is placed,
  when the placement timeout ends, or when its launch fails.
- The reply to an open ("opened") goes out when each group of slots has one
  launch that started, or has no slots left. The reply is an error only when
  no launch started. Browser slots that start after the reply write a failure
  to the log only.
- If the saved browser profile does not exist, or vdwd cannot read the
  profile list of the browser, the browser opens its last-used profile.
  vdwd then writes a warning to its log and sends a notification that names
  the app and the profile (one for each slot). vdwd never tells the browser
  to use a profile that does not exist, because the browser then makes a new
  empty profile.
- The profile of a slot has priority over a `--profile-directory` option in
  its arguments. Capture does not save a profile.

## The main shortcut: Ctrl+Meta+D

| Gesture | Result |
|---|---|
| Tap (press and release quickly) | Open the default workspace (with the confirm flow above if the desktop is busy) |
| Hold (still pressed after a short threshold, 250 ms, configurable) | Show the radial menu |
| Tap D again while a confirm message is showing | Confirm |

If no default workspace is set (or it was deleted), a tap shows a short
toast instead (it hides after 3 seconds); holding still opens the menu.

Per-workspace shortcuts only support tap (with the confirm flow above, where
only the timeout cancels); they never open the menu.

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
- Restoring app state (open files, tabs, terminal folders)
- Moving or closing windows that were already on the desktop
- Activities
