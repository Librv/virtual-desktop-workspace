# Virtual Desktop Workspace

KDE Plasma 6 (Wayland) tool that opens saved sets of apps at saved positions on
the current virtual desktop, via global shortcuts and a radial menu.

## Read first

- `docs/SPEC.md`: what the app does (source of truth for behavior)
- `docs/ARCHITECTURE.md`: components and how they talk
- `docs/DECISIONS.md`: settled choices and open questions
- `docs/tasks/README.md`: task list, dependencies, status

## Working on a task

1. Pick a task in `docs/tasks/README.md` whose dependencies are `done`; set it
   to `in-progress`.
2. When other tasks run in parallel, work in your own git worktree.
3. Spikes: put throwaway code in `spikes/<id>/` and findings in
   `docs/spikes/<id>.md`. Update `ARCHITECTURE.md` if the findings change the
   design.
4. Record new settled choices in `DECISIONS.md` (append, do not rewrite).
5. Set the task to `done` only when its acceptance criteria pass; say which
   ones were checked by hand and which need the user.

## Agent workflow

The coordinator (main session) plans, decides and integrates; subagents do
bounded work:

- `scout`: read-only searches and "how does X work" questions.
- `implementer`: one task, or one well-scoped part of it, with the design
  already decided in these docs. Implementers do not commit and do not make
  design decisions: if the docs are ambiguous or wrong, they stop and report.
  Never run two implementers on overlapping files; parallel implementers each
  get their own git worktree.
- `reviewer`: run on every finished task before it is marked done. The
  coordinator commits after the review findings are handled.
- Design questions go to the coordinator, and from there to the user when
  they change behavior.

## Rules

- Never use em-dashes (U+2014) anywhere: code, comments, docs, commits.
- Do not run anything that moves, launches or closes windows on the user's
  live session. Use the nested KWin session (`scripts/dev-session.sh`, from
  T0b). Live-session checks are written down as steps for the user.
- Only change files inside this repo. Installing packages or touching
  `~/.config`, `~/.local` or other folders needs the user's OK.
- Target: Plasma 6.7 / KWin 6.7, Qt 6, KF6, C++20, single monitor.

## Code style

- C++: KDE style (`.clang-format` from T0b), Qt naming conventions.
- Keep logic that can be pure (gesture state machine, layout math, slice
  selection, config parsing) free of GUI and D-Bus so it is unit-testable.

## Build and test

Needs `extra-cmake-modules` and `ninja` installed.

```
scripts/build.sh test     # configure if needed, build, run unit tests
scripts/build.sh format   # format C++ sources
scripts/build.sh clean    # wipe build/dev
```

`scripts/build.sh install` (makepkg + pacman, needs sudo, restarts vdwd on the
live session) is for the user only: suggest it, never run it.

Nested KWin session (details and known gaps in `docs/spikes/T0b.md`):

```
scripts/dev-session.sh start            # headless 1920x1080 KWin, own bus/HOME
scripts/dev-session.sh run -- konsole   # run anything against it
scripts/dev-session.sh script foo.js    # run a KWin script, print its output
scripts/dev-session.sh selftest
scripts/dev-session.sh stop
```

The Claude Code sandbox blocks Unix sockets, so these only work when
`scripts/dev-session.sh` runs outside it (`sandbox.excludedCommands` entry
`scripts/dev-session.sh *`; see `docs/spikes/T0b.md`). Run each as a single
command, not after `cd` or `&&`. Never point `kdotool`/`qdbus6` at the default
bus: that is the live session.

Several agents can run nested sessions at once: give each its own state dir,
e.g. `VDW_DEV_DIR=/run/user/1000/vdw-<task> scripts/dev-session.sh start`.
Fake key/pointer input and screenshots work in the nested session (see
`docs/ARCHITECTURE.md`, Testing).
