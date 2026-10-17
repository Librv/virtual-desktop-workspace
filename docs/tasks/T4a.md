# T4a: Radial menu component

**Depends on:** T1. **Owns:** `src/menu/` only (its own `CMakeLists.txt`,
static library `vdwmenu`, QML, a demo binary and tests). Do not edit any file
outside `src/menu/`.

Design: `docs/ARCHITECTURE.md` "Radial menu", DECISIONS 10 and 11,
`docs/spikes/S2.md` and the prototype in `spikes/S2/`.

## Scope

- `RadialMath` from `spikes/S2/radialmath.{h,cpp}` with its tests, as is
  (namespace and file names may follow the project's style).
- `RadialMenu` class: owns a transparent full-screen `QQuickView` configured
  through LayerShellQt (overlay layer, all anchors, exclusive zone -1,
  keyboard interactivity none). API: `show(QPoint center, items)` where an
  item is `{id, name, icon}`, `hide()`, and `selectedId()` returning the id
  under the last pointer position or empty for the dead zone. Created on
  demand on `show`, destroyed on `hide`.
- QML from `spikes/S2/Overlay.qml`: Kirigami theme colors, icon and name per
  slice, highlight follows the pointer angle, center dead zone, empty state
  "No workspaces yet". No Esc handling.
- `vdw-menu-demo` binary (not installed) for nested-session checks: shows
  the menu at a given center with sample items, prints the selected id when
  told to (stdin line), like the spike.

## Acceptance

- Radial math tests pass.
- In the nested session, using the spike's fake input and screenshot tools
  (`spikes/S2/tools/`, authorization as in `spikes/S2/nested-authorize.sh`):
  the demo renders, hovering highlights the right slice, and the reported
  selection matches; screenshots in light and dark theme saved under
  `docs/spikes/T4a/`.
