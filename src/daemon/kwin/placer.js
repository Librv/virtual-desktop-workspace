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

// internalIds of the windows sent to ClaimWindow that are still open. Only
// their closes go to ReleaseWindow.
let reported = new Set();

// Set the rect the daemon named and raise the windows of the claim's group
// from the lowest slot up, so the last slot ends on top. Other windows and the
// focus are left alone.
function applyClaim(w, claim) {
    // Setting frameGeometry sticks (spike S3).
    w.frameGeometry = {x: claim.rect.x, y: claim.rect.y, width: claim.rect.width, height: claim.rect.height};
    claimed = claimed.filter(function(c) {
        return !c.w.deleted && c.w !== w;
    });
    claimed.push({w: w, group: claim.group, order: claim.order});
    // Windows arrive in any order.
    claimed.filter(function(c) {
        return c.group === claim.group;
    }).sort(function(a, b) {
        return a.order - b.order;
    }).forEach(function(c) {
        workspace.raiseWindow(c.w);
    });
}

workspace.windowAdded.connect(function(w) {
    if (!isAppWindow(w)) {
        return;
    }
    const id = w.internalId.toString();
    reported.add(id);
    const info = {
        id: id,
        pid: w.pid,
        desktopFileName: w.desktopFileName,
        resourceClass: w.resourceClass,
        resizeable: w.resizeable,
    };
    callDBus(SERVICE, PATH, IFACE, "ClaimWindow", JSON.stringify(info), function(reply) {
        if (!reply || w.deleted) {
            return;
        }
        applyClaim(w, JSON.parse(reply));
    });
});

// The daemon needs every close, also of windows it did not place: a startup
// window can close before it was claimed. A non-empty reply names the window
// that takes the slot of the closed one.
workspace.windowRemoved.connect(function(w) {
    const id = w.internalId.toString();
    if (!reported.delete(id)) {
        return;
    }
    claimed = claimed.filter(function(c) {
        return c.w !== w && !c.w.deleted;
    });
    callDBus(SERVICE, PATH, IFACE, "ReleaseWindow", JSON.stringify({id: id}), function(reply) {
        if (!reply) {
            return;
        }
        const handover = JSON.parse(reply);
        const target = workspace.windowList().find(function(x) {
            return x.internalId.toString() === handover.id;
        });
        if (!target || target.deleted) {
            return;
        }
        applyClaim(target, handover);
    });
});
