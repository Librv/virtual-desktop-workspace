# Virtual Desktop Workspace

Open a saved set of apps at saved positions on the current virtual desktop, with
a global shortcut or a radial menu. For KDE Plasma 6 on Wayland (single monitor).

## What it does

A workspace is a named layout: a list of apps (by `.desktop` id, with optional
arguments) and a position and size for each, stored as fractions of the screen
area not covered by panels. The background daemon `vdwd` opens a workspace on the
current virtual desktop; nothing else is moved or closed.

- **Tap Ctrl+Meta+D:** open the default workspace.
- **Hold Ctrl+Meta+D:** show the radial menu around the pointer. Move toward a
  slice and release to open that workspace; release in the center to cancel.
- **Confirm:** if the desktop already has windows, nothing launches yet and a
  Plasma on-screen message asks you to tap D again (within 2 seconds) to set up
  anyway. New windows go into their slots even if they overlap existing ones.
- **Per-workspace shortcuts:** each workspace can have its own shortcut (tap only,
  same confirm flow).
- **Capture:** arrange real windows, then save the current desktop as a new
  workspace (the "Save current desktop as workspace" button in the settings page,
  or `vdwctl capture <name>`).
- **Settings page:** System Settings, Virtual Desktop Workspaces. Add, reorder
  and edit workspaces and their apps, pick the default, and configure shortcuts
  (the same entries appear in System Settings, Shortcuts).

Behavior details are in `docs/SPEC.md`, the design in `docs/ARCHITECTURE.md`.

## Build

Needs `extra-cmake-modules` and `ninja`, plus the KF6, Qt 6 and LayerShellQt
development packages.

```
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
```

## Install (Arch, CachyOS)

```
cd packaging/arch
makepkg -si
```

This builds from the local checkout and installs `vdwd` and `vdwctl`, the D-Bus
service file, an XDG autostart entry and the System Settings page.

## First run

Log out and in, or run `vdwctl list` once: vdwd is D-Bus activated, so any
`vdwctl` call starts it with your Plasma session's environment. Do not start
`vdwd` directly from a terminal: apps it launches inherit that terminal's
environment (for example a different `SHELL`, so Konsole opens the wrong
shell). On the first start, if `~/.config/vdwrc`
does not exist, vdwd writes a starting workspace "3 Konsoles" (three Konsole
windows in the top-right, bottom-left and bottom-right quadrants) and makes it the
default, so Ctrl+Meta+D works right away. An existing `vdwrc` is never touched.

`vdwctl list` prints the workspaces as JSON; `vdwctl open <id> [--force]` opens one.
A step by step check of the installed app is in `docs/LIVE-CHECKS.md`.

## Uninstall

```
sudo pacman -R vdw
```

Your `~/.config/vdwrc` stays; delete it by hand if you want it gone.

## Migrating from setup-workspace.sh

The old bash script and vdwd cannot both own Ctrl+Meta+D. Before the first run:

1. In System Settings, Shortcuts, Custom Shortcuts, remove the Meta+Ctrl+D entry
   for the script.
2. `rm ~/.local/share/applications/net.local.setup-workspace.sh.desktop`
3. `rm ~/.local/bin/setup-workspace.sh`

`kdotool` is no longer needed.

## Project layout

```
src/lib/         config reading and writing, layout math (no GUI, no D-Bus)
src/daemon/      vdwd: D-Bus API, launching, KWin scripts, shortcuts, gestures
src/gesture/     tap/hold/confirm state machine
src/menu/        radial menu overlay
src/kcm/         System Settings page
src/vdwctl/      command line client
packaging/arch/  PKGBUILD
docs/            spec, architecture, decisions, tasks, spikes
```
