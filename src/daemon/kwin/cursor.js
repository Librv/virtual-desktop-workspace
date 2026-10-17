// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

// One-shot query: where is the pointer? vdwd unloads the script afterwards.
(function() {
    const pos = workspace.cursorPos;
    callDBus("io.github.librv.vdw", "/Script", "io.github.librv.vdw.Script", "CursorPos",
             Math.round(pos.x), Math.round(pos.y));
})();
