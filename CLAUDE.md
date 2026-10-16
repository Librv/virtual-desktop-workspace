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

## Rules

- Never use em-dashes (U+2014) anywhere: code, comments, docs, commits.
- Do not run anything that moves, launches or closes windows on the user's
  live session. Use the nested KWin session (`scripts/dev-session.sh`, from
  T0b). Live-session checks are written down as steps for the user.
- Only change files inside this repo. Installing packages or touching
  `~/.config`, `~/.local` or other folders needs the user's OK.
- `setup-workspace.sh` is the working legacy version. Keep it working until T7
  replaces it.
- Target: Plasma 6.7 / KWin 6.7, Qt 6, KF6, C++20, single monitor.

## Code style

- C++: KDE style (`.clang-format` from T0b), Qt naming conventions.
- Keep logic that can be pure (gesture state machine, layout math, slice
  selection, config parsing) free of GUI and D-Bus so it is unit-testable.

## Build and test

Set up in T0b. Expected: `cmake --preset dev`, `cmake --build --preset dev`,
`ctest --preset dev`.
