# T3a: Gesture state machine library

**Depends on:** T1. **Owns:** `src/gesture/` only (its own `CMakeLists.txt`,
static library `vdwgesture`, its own tests registered with `ecm_add_test`
under `if(BUILD_TESTING)`). Do not edit any file outside `src/gesture/`.

Behavior: `docs/SPEC.md` (Opening a workspace, The main shortcut, Radial
menu), `docs/DECISIONS.md` rows 5, 7 to 10, `docs/spikes/S1.md` section
"Recommendation for T3" (edge cases).

## Design (fixed)

A pure class (Qt Core only, no timers of its own, no GUI, no D-Bus):

- Inputs: `press(shortcut)`, `release(shortcut)`, `modifiersHeld(bool)`,
  `timerFired(token)`, `menuResult(optional workspaceId)` (the selection at
  release time, empty for the dead zone), `openResult(workspaceId, busy)`.
  `shortcut` is either the main shortcut or a workspace id.
- Outputs, returned as a list of commands per input (or emitted through a
  callback; pick one and keep it simple): `StartTimer(token, ms)`,
  `ShowMenu`, `HideMenu`, `Open(workspaceId, force)`,
  `ShowConfirmOsd(workspaceId)`.
- Configuration: default workspace id, hold threshold (250 ms), confirm
  timeout (2000 ms).
- Rules:
  - main shortcut press: start hold timer. Release before it fires: tap,
    `Open(default, false)`. Timer fires while pressed: `ShowMenu`.
  - menu open: release ends the hold; the daemon then calls `menuResult`;
    a workspace id gives `HideMenu` and `Open(id, false)`, empty gives
    `HideMenu` only.
  - workspace shortcut: release gives `Open(id, false)`; never a menu.
  - `openResult(id, busy=true)`: enter awaiting-confirm for `id`, remember
    whether it came from a tap or the menu, `ShowConfirmOsd(id)`, start the
    confirm timer.
  - awaiting confirm: a press of the same shortcut that triggered it (main
    shortcut for the menu path) gives `Open(id, true)` and returns to idle
    (the matching release is swallowed). Timer firing cancels.
    `modifiersHeld(false)` cancels only in the tap path.
  - Robustness: release without press is ignored; a press without a
    release in between restarts from pressed; stale timer tokens are
    ignored (generation counter).

## Acceptance

- Tests with explicit inputs and expected command lists for every rule and
  edge case above, including both confirm paths, both cancel paths, stale
  timers, and per-workspace shortcuts.
