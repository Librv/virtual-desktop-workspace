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
| 22 | 2026-10-17 | Install through a PKGBUILD (`makepkg -si`, remove with `pacman -R`) | System Settings finds the KCM only in standard locations; clean uninstall. User accepted |
| 23 | 2026-10-17 | Start vdwd with an XDG autostart entry plus D-Bus activation | Plasma turns autostart entries into systemd units itself; D-Bus activation covers a crashed or not yet started daemon. User accepted |
| 24 | 2026-10-17 | On first start vdwd writes a "3 Konsoles" default workspace (the legacy layout) to `~/.config/vdwrc` if the file is missing | Ctrl+Meta+D keeps doing what the bash script did; `/etc/xdg` defaults are not read because vdwrc is opened with SimpleConfig. User accepted |
| 25 | 2026-10-17 | Retire `setup-workspace.sh`: delete it from the repo, migration steps remove the old shortcut, `.desktop` file and script | Both cannot own Ctrl+Meta+D. User accepted |
| 26 | 2026-10-17 | Supersedes 9 and the OSD parts of 2 and 19. Confirm only by keeping Ctrl+Meta held and pressing D again, after a tap and after a menu pick; modifier release or the 2 s timeout cancels; already released modifiers cancel without a toast. Workspace shortcuts keep the timeout-only cancel | User decision after the first live test |
| 27 | 2026-10-17 | Messages use our own layer-shell toast centered at the top of the screen instead of Plasma's OSD | Plasma's OSD position cannot be changed; user wants the toast at the top center |
| 28 | 2026-10-17 | KCM save first merges workspaces that appeared in `vdwrc` since its last load or save (e.g. `vdwctl capture`); workspaces deleted in the KCM stay deleted. The placer no longer reports `WindowPlaced` | Saving dropped captures made while the KCM was open (and their shortcuts); `WindowPlaced` was only logged (audit) |
| 29 | 2026-10-17 | Capture leaves out the windows of the D-Bus caller's process (System Settings, `kcmshell6`); `vdwctl capture` keeps the terminal it runs in | The settings window was saved as a slot of every workspace captured from the KCM. User request |
| 30 | 2026-10-17 | The workspace placement preview (KCM only) draws slots on a mini screen at the current screen's aspect ratio, one fixed color per slot, not from the window palette | Screen size is not stored, only fractions. Palette colors dim when System Settings is unfocused. User does not want it in the radial menu |
| 31 | 2026-10-17 | vdwd draws the menu and toast with Qt Quick's software renderer (`QSGRendererInterface::Software`) | With the GPU renderer Mesa stays loaded after the first menu: private memory went from 7 MB to 46 MB, software keeps it at 13 MB. Screenshots looked the same (nested session) |
| 32 | 2026-10-18 | vdwd has no Qt GUI: menu, toast, modifier tracking and launching run in a short-lived `vdw-ui` helper (one per gesture or launch); shortcuts go over raw D-Bus to kglobalaccel (S5) | Idle vdwd drops from 49 MB RSS (7 MB private) to 18 MB (1.6 MB) and stays there after menu use; the 65 ms helper start hides behind the 250 ms hold threshold. User tested it live |

## Open

- None right now.
