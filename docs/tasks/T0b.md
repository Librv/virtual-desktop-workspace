# T0b: Dev environment

**Depends on:** T0

## Goal

Let agents build and test without touching the user's live desktop.

## Scope

- `scripts/dev-session.sh`: start a headless nested KWin
  (`dbus-run-session kwin_wayland --virtual --xwayland`, fixed size, e.g.
  1920x1080) and print or export the D-Bus address and `WAYLAND_DISPLAY` so
  other commands can target it. Also a way to stop it.
- Check what works inside the nested session: KWin scripting over D-Bus,
  `kglobalaccel` (built into KWin 6?), launching Konsole into it. Note gaps in
  `docs/spikes/T0b.md`.
- Top-level `CMakeLists.txt` skeleton (Qt 6, KF6 via ECM) that builds an empty
  test target, plus `CMakePresets.json` with a `dev` preset and `build/` in
  `.gitignore`.
- `.clang-format` in KDE style.
- Ask the user before installing anything outside the project (e.g.
  `shellcheck`).

## Acceptance

- One command starts the nested session, one stops it, and a Konsole launched
  into it is visible to a KWin script run against that session.
- `cmake --preset dev && cmake --build --preset dev && ctest --preset dev`
  succeeds.
