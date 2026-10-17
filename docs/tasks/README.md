# Tasks

One file per task. An agent picks a task whose dependencies are all `done`,
sets it to `in-progress` here, and works in its own git worktree when other
tasks run at the same time. Spikes write their findings to `docs/spikes/<id>.md`
and may change `ARCHITECTURE.md`; settled choices go into `DECISIONS.md`.

Status values: `todo`, `in-progress`, `blocked`, `done`.

| ID | Title | Depends on | Wave | Status |
|---|---|---|---|---|
| T0 | Specs, task files, CLAUDE.md | none | | done |
| T0b | Dev environment: nested KWin, CMake skeleton, linters | T0 | | done |
| Q1 | Bash script: busy-desktop OSD + confirm | T0 | | done (live check by user pending) |
| S1 | Spike: KGlobalAccel tap/hold/confirm | T0b | | done |
| S2 | Spike: radial overlay on LayerShellQt | T0b | | done |
| S3 | Spike: KWin script placement via events | T0b | | done |
| S4 | Spike: launching and window matching | T0b | | done |
| T1 | Daemon skeleton, config, D-Bus API, vdwctl | S1, S3 | | done |
| T2 | Launch and placement engine (daemon core) | T1 | 1 | done |
| T3a | Gesture state machine library | T1 | 1 | done |
| T4a | Radial menu component | T1 | 1 | done |
| T5 | KCM (System Settings page) | T1 | 1 | done |
| T3b | Wire shortcuts, gesture machine, OSD and menu into vdwd | T2, T3a, T4a | 2 | done |
| T6 | Save current desktop as workspace | T2 | 2, after T3b | todo |
| T7 | Packaging, autostart, migrate off the bash script | all | 3 | todo |

## File ownership (no two parallel tasks touch the same file)

| Task | Owns |
|---|---|
| T2 | `src/lib/`, `src/daemon/`, `src/vdwctl/`, `autotests/` |
| T3a | `src/gesture/` (own library and tests) |
| T4a | `src/menu/` (own library, QML, demo binary and tests) |
| T5 | `src/kcm/` (own plugin and tests) |
| T3b, T6 | `src/daemon/` and `autotests/`, one after the other |

The top-level `CMakeLists.txt` already adds `src/gesture`, `src/menu` and
`src/kcm` when they exist; wave 1 tasks must not edit it.

T4 (old) was split into T4a (component) and wiring inside T3b, because the
gesture machine drives the menu.
