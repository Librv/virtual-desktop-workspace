// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

// One-shot query: the app windows of the current desktop, bottom to top in
// stacking order (same filter as empty.js). vdwd replaces the @...@
// placeholders before loading, and unloads the script afterwards. Uses
// windowList(), not stackingOrder: the latter keeps just-closed windows around.
(function() {
    const ownPid = @OWN_PID@;
    const desktop = workspace.currentDesktop;
    const stacking = workspace.stackingOrder;
    const windows = workspace.windowList().filter(function(w) {
        return w.normalWindow && w.minimizable && w.pid !== ownPid
            && (w.onAllDesktops || w.desktops.indexOf(desktop) !== -1);
    }).sort(function(a, b) {
        return stacking.indexOf(a) - stacking.indexOf(b);
    }).map(function(w) {
        const g = w.frameGeometry;
        return {
            x: g.x, y: g.y, width: g.width, height: g.height,
            desktopFileName: w.desktopFileName,
            resourceClass: w.resourceClass,
            caption: w.caption,
        };
    });
    callDBus("io.github.librv.vdw", "/Script", "io.github.librv.vdw.Script", "QueryResult",
             "@REQUEST_ID@", JSON.stringify({ windows: windows }));
})();
