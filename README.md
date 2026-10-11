# Virtual Desktop Workspace

Open and place a predefined set of apps on an empty virtual desktop, triggered by a
global shortcut, on KDE Plasma 6 (Wayland).

## Current behavior

- Checks that the current virtual desktop has zero windows (including minimized ones)
- If empty, launches 3 Konsole windows and places them as flush quadrants:
  top-right, bottom-left, bottom-right (top-left intentionally left empty)
- Panel-aware: uses KWin's own usable-area calculation, so placement never overlaps
  the panel
- Bound to a global shortcut via a `.desktop` file + KDE's Custom Shortcuts

## Requirements

- KDE Plasma 6, Wayland
- [`kdotool`](https://github.com/jinliu/kdotool) — drives KWin's scripting API over
  D-Bus; not in official Arch/CachyOS repos, only the AUR (`kdotool-bin` is the
  prebuilt package)

## Installation

```bash
mkdir -p ~/.local/bin
cp setup-workspace.sh ~/.local/bin/
chmod +x ~/.local/bin/setup-workspace.sh
```

Bind it to a global shortcut via System Settings → Shortcuts → Custom Shortcuts
(Command/URL), pointing at the full path above.

## Known limitations

- Shortcut registration goes through `kglobalaccel` via a `.desktop` file, which has
  been unreliable in practice: it appears to cache the invoked command at
  registration time rather than re-reading the `.desktop` file's `Exec=` on every
  press, so changes sometimes require deleting and recreating the shortcut rather
  than just editing the file. Phase 2 below replaces this mechanism entirely.
- Only one hardcoded layout (3 Konsoles, 3 corners) — not yet configurable.
- Requires a genuinely empty desktop; does not yet find/create one automatically if
  the current one is occupied.

## Project layout

```
setup-workspace.sh   # current implementation (Phase 0/1)
```
