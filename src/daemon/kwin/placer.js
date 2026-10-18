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

// Windows claimed by a workspace open: {w, group, order}. Used to restack
// the windows of one open in slot order (last slot on top).
let claimed = [];

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
        const claim = JSON.parse(reply);
        // Setting frameGeometry sticks (spike S3).
        w.frameGeometry = {x: claim.rect.x, y: claim.rect.y, width: claim.rect.width, height: claim.rect.height};
        claimed = claimed.filter(function(c) {
            return !c.w.deleted;
        });
        claimed.push({w: w, group: claim.group, order: claim.order});
        // Windows arrive in any order: raise the group's windows from the
        // lowest slot up, so the last slot ends on top. Other windows and the
        // focus are left alone.
        claimed.filter(function(c) {
            return c.group === claim.group;
        }).sort(function(a, b) {
            return a.order - b.order;
        }).forEach(function(c) {
            workspace.raiseWindow(c.w);
        });
    });
});
