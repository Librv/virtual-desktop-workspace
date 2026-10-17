# T3b: Wire shortcuts, gesture machine, OSD and menu into vdwd

**Depends on:** T2, T3a, T4a. **Owns:** `src/daemon/`, `autotests/`.

Spec to be finalized by the coordinator after wave 1 lands. Outline:
KGlobalAccel registration (component `vdw`, `main` plus `open-<id>`, flags
from S1), `KModifierKeyInfo` for the modifier set of each shortcut, the
`vdwgesture` machine with real `QTimer`s, `RadialMenu` with its center from a
`CursorPos` one-shot script, Plasma OSD, `/Debug` feeding the machine, and
re-registering shortcuts on `ReloadConfig`. Integration tests drive real key
events with `spikes/S1/s1inject` in the nested session.
