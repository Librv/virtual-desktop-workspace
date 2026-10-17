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

function roundedRect(r) {
    return { x: Math.round(r.x), y: Math.round(r.y), width: Math.round(r.width), height: Math.round(r.height) };
}

function sameRect(a, b) {
    return Math.round(a.x) === b.x && Math.round(a.y) === b.y && Math.round(a.width) === b.width && Math.round(a.height) === b.height;
}

function place(w, rect) {
    let reported = false;
    function report() {
        if (reported) {
            return;
        }
        reported = true;
        const r = roundedRect(w.frameGeometry);
        callDBus(SERVICE, PATH, IFACE, "WindowPlaced", JSON.stringify({ id: w.internalId.toString(), x: r.x, y: r.y, width: r.width, height: r.height }));
    }
    if (sameRect(w.frameGeometry, rect)) {
        report();
        return;
    }
    // Setting frameGeometry sticks (spike S3); the change signal confirms it.
    w.frameGeometryChanged.connect(report);
    w.frameGeometry = { x: rect.x, y: rect.y, width: rect.width, height: rect.height };
    if (sameRect(w.frameGeometry, rect)) {
        report();
    }
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
        place(w, JSON.parse(reply));
    });
});
