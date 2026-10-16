# Decisions

Settled choices, newest last. Add an entry when a choice is made; do not
rewrite old entries, add a new one that supersedes them.

| # | Date | Decision | Why |
|---|---|---|---|
| 1 | 2026-10-17 | Workspaces open on the current virtual desktop; no desktop creation | User preference |
| 2 | 2026-10-17 | Busy desktop: OSD message, confirm by tapping the last key again while holding modifiers; timeout fallback | Intuitive with Ctrl+Meta+D; strict modifier-release detection may not be possible on Wayland (S1) |
| 3 | 2026-10-17 | C++ / Qt 6 / KF6 | KGlobalAccel, LayerShellQt and KCMs are C++/QML first; Python bindings are missing or partial |
| 4 | 2026-10-17 | Build our own radial menu (QML + LayerShellQt), not Kando | Plasma-native look, no Electron dependency, full control over the tap/hold gesture |
| 5 | 2026-10-17 | Ctrl+Meta+D: tap opens default workspace, hold opens radial menu | User preference |
| 6 | 2026-10-17 | Single monitor only for now | User preference |
| 7 | 2026-10-17 | Every way of opening a workspace on a busy desktop asks for confirmation, including radial menu picks | User preference |
| 8 | 2026-10-17 | Hold threshold 250 ms, confirm timeout 2 s, both configurable | User accepted the proposal |

## Open

- Config schema and D-Bus names (T1).
