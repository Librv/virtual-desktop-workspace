# Tasks

One file per task. An agent picks a task whose dependencies are all `done`,
sets it to `in-progress` here, and works in its own git worktree when other
tasks run at the same time. Spikes write their findings to `docs/spikes/<id>.md`
and may change `ARCHITECTURE.md`; settled choices go into `DECISIONS.md`.

Status values: `todo`, `in-progress`, `blocked`, `done`.

| ID | Title | Depends on | Parallel group | Status |
|---|---|---|---|---|
| T0 | Specs, task files, CLAUDE.md | none | | done |
| T0b | Dev environment: nested KWin, CMake skeleton, linters | T0 | A | done |
| Q1 | Bash script: busy-desktop OSD + confirm | T0 | A | done (live check by user pending) |
| S1 | Spike: KGlobalAccel tap/hold/confirm | T0b | B | done |
| S2 | Spike: radial overlay on LayerShellQt | T0b | B | done |
| S3 | Spike: KWin script placement via events | T0b | B | done |
| S4 | Spike: launching and window matching | T0b | B | done |
| T1 | Daemon skeleton, config, D-Bus API, vdwctl | S1, S3 | | in-progress |
| T2 | Launch and placement engine | T1, S3, S4 | C | todo |
| T3 | Shortcuts, gesture state machine, OSD | T1, S1 | C | todo |
| T4 | Radial menu | T1, S2 | C | todo |
| T5 | KCM (System Settings page) | T1 | C | todo |
| T6 | Save current desktop as workspace | T1, S3 | C | todo |
| T7 | Packaging, autostart, migrate off the bash script | T2, T3, T4, T5, T6 | | todo |

Tasks in the same parallel group can run at the same time once their
dependencies are done. T4 also needs T3's gesture events to be wired up for the
end-to-end test, but the menu itself can be built against the D-Bus debug
methods first.
