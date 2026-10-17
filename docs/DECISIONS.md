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
| 9 | 2026-10-17 | Supersedes the "timeout fallback" part of 2. Confirm: a new press of the same shortcut while a confirm is pending confirms. After a tap, releasing Ctrl/Meta cancels immediately; after a radial menu pick, only the 2 s timeout cancels | Modifier release is detectable on Wayland (S1), but a menu pick happens on release, so a strict rule would make that confirm unreachable. User accepted |
| 10 | 2026-10-17 | No Esc in the radial menu; cancel by releasing in the center dead zone | With Ctrl+Meta held, Esc becomes KWin's Kill Window shortcut (bound by default and in the user's config); the overlay also gets no keys without taking focus (S2). User accepted |
| 11 | 2026-10-17 | Radial menu overlay: keyboard interactivity none, center from KWin `workspace.cursorPos` | No focus theft; Wayland clients cannot read the pointer position and the surface gets no enter event for a resting pointer (S2) |
| 12 | 2026-10-17 | KWin script pulls placements with `callDBus` (`ClaimWindow`) on `windowAdded`; queries run as one-shot scripts; no shortcut doorbell, no script config | No push channel into a loaded script; `registerShortcut` persists in the user's config; `readConfig` ignores external writes (S3) |
| 13 | 2026-10-17 | Window matching: pid first, then `desktopFileName` in launch order, pending entries expire after 10 s; no activation tokens, no captions | Measured in S4; tokens are not exposed to KWin scripts |
| 14 | 2026-10-17 | D-Bus service `io.github.librv.vdw`, JSON strings for structured data; config in KConfig `vdwrc`, shortcuts owned by KGlobalAccel component `vdw` | Named after the GitHub repo; `callDBus` handles strings well and complex types badly (S3) |
| 15 | 2026-10-17 | Agent workflow follows the user's global rules: implementers do not commit, a reviewer pass before a task is done, design decisions stay with the coordinator or the user | User's CLAUDE.md |
| 16 | 2026-10-17 | KWin scripts are started with `org.kde.kwin.Scripting.start()`, never `Script<id>.run` | KWin reuses script ids after an unload, so `run` by id can start the wrong script (found in T2) |
| 17 | 2026-10-17 | `OpenWorkspace` also replies `busy` (unless forced) while launches from an earlier open are still pending | The desktop looks empty until the windows appear, so a quick double press would launch everything twice; the confirm flow covers the intentional case (T2) |
| 18 | 2026-10-17 | On startup vdwd removes `open-<id>` shortcuts whose workspace no longer exists; `main` is never touched | A workspace deleted while the daemon was not running would otherwise leave a dead entry in System Settings (T3b review) |
| 19 | 2026-10-17 | A tap with no usable default workspace shows an OSD hint instead of doing nothing | Silent failure is confusing; holding still opens the menu (T3b review) |
| 20 | 2026-10-17 | Saving (KCM or capture) rewrites every `[Workspace]` group through the shared writer; entries the loader rejects are dropped | One writer for both paths; the KCM shows the loader's warnings before a save (T5, T6) |
| 21 | 2026-10-17 | KWin scripts list windows from `workspace.windowList()`; `stackingOrder` is only a sort key | `stackingOrder` still contains just-closed windows (T6) |

## Open

- None right now.
