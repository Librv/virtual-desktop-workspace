// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

// Long-lived KWin script loaded by vdwd at startup (docs/ARCHITECTURE.md,
// "KWin script"). The daemon decides, this script only reports new windows and
// applies the rect the daemon names.

const SERVICE = "io.github.librv.vdw";
const PATH = "/Script";
const IFACE = "io.github.librv.vdw.Script";

// App window: normalWindow alone also matches layer-shell surfaces
// (the radial menu), which are not minimizable.
function isAppWindow(w) {
    return w.normalWindow && w.minimizable;
}

workspace.windowAdded.connect(function(w) {
    if (!isAppWindow(w)) {
        return;
    }
    const info = {
        id: w.internalId.toString(),
        pid: w.pid,
        desktopFileName: w.desktopFileName,
        resourceClass: w.resourceClass,
    };
    callDBus(SERVICE, PATH, IFACE, "ClaimWindow", JSON.stringify(info), function(reply) {
        if (!reply || w.deleted) {
            return;
        }
        // Setting frameGeometry sticks (spike S3).
        w.frameGeometry = JSON.parse(reply);
    });
});
