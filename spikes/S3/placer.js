// Spike S3: long-lived, event-driven KWin script (stand-in for the one vdwd
// will load). Talks to the throwaway receiver (spikes/S3/receiver.py) on the
// nested bus. Throwaway code.
//
// Work in:  - on windowAdded the script asks the receiver (ClaimWindow) whether
//             a placement is pending for this window; reply = rect or "".
//           - a registered shortcut "VDW S3 Poke" acts as a doorbell: the
//             daemon invokes it through kglobalaccel, the script then pulls
//             queued commands with TakeWork.
// Results out: Report(json) / Log(text) via callDBus.

const SVC = "org.kde.vdw.S3";
const PATH = "/S3";
const IFACE = "org.kde.vdw.S3";
const T0 = Date.now();

// Placements pushed ahead of time ("prefetch" mode), applied synchronously
// inside windowAdded without a D-Bus round trip.
let prefetched = [];

function ms() {
    return Date.now() - T0;
}

function log(msg) {
    callDBus(SVC, PATH, IFACE, "Log", "[js +" + ms() + "ms] " + msg);
}

function report(obj) {
    callDBus(SVC, PATH, IFACE, "Report", JSON.stringify(obj));
}

function rect(r) {
    return { x: Math.round(r.x), y: Math.round(r.y), width: Math.round(r.width), height: Math.round(r.height) };
}

function rectStr(r) {
    return Math.round(r.x) + "," + Math.round(r.y) + " " + Math.round(r.width) + "x" + Math.round(r.height);
}

// Desktop the daemon targets: the current one. (Plasma 6.7 also has
// per-screen current desktops; single monitor, so these are the same.)
function targetDesktop() {
    return workspace.currentDesktop;
}

function onDesktop(w, d) {
    // An empty desktops list means "on all desktops" (onAllDesktops).
    return w.onAllDesktops || w.desktops.indexOf(d) !== -1;
}

// "Real" app window. normalWindow alone is not enough: in KWin 6.7 a plain
// layer-shell surface (no Plasma panel role) is normalWindow=true and
// onAllDesktops=true, but it is not minimizable (same test as KWin's own
// minimizeall script). The runs logged in docs/spikes/S3.md used
// normalWindow only; oneshot-query.js verified this filter.
function isAppWindow(w) {
    return w.normalWindow && w.minimizable;
}

function windowInfo(w) {
    return {
        id: w.internalId.toString(),
        desktopFileName: w.desktopFileName,
        resourceClass: w.resourceClass,
        resourceName: w.resourceName,
        caption: w.caption,
        pid: w.pid,
        normalWindow: w.normalWindow,
        managed: w.managed,
        deleted: w.deleted,
        minimized: w.minimized,
        onAllDesktops: w.onAllDesktops,
        desktops: w.desktops.map(function(d) { return d.name; }),
        frameGeometry: rect(w.frameGeometry),
        clientGeometry: rect(w.clientGeometry),
        maximizable: w.maximizable,
        resizeable: w.resizeable,
        moveable: w.moveable,
        minSize: { width: w.minSize.width, height: w.minSize.height },
        output: w.output ? w.output.name : null,
    };
}

// Port of setup-workspace.sh step 1.
function checkEmpty() {
    const d = targetDesktop();
    const blockers = workspace.windowList().filter(function(w) {
        return isAppWindow(w) && onDesktop(w, d);
    });
    return { empty: blockers.length === 0, count: blockers.length,
             desktop: d.id, desktopName: d.name };
}

// Port of setup-workspace.sh step 2.
function usableArea() {
    const d = targetDesktop();
    const a = workspace.clientArea(KWin.PlacementArea, workspace.activeScreen, d);
    return { desktop: d.id, screen: workspace.activeScreen.name, area: rect(a) };
}

// For T6: normal windows on the current desktop, bottom to top.
function listWindows() {
    const d = targetDesktop();
    const area = usableArea().area;
    return {
        desktop: d.id,
        area: area,
        windows: workspace.stackingOrder.filter(function(w) {
            return isAppWindow(w) && onDesktop(w, d);
        }).map(function(w) {
            const g = w.frameGeometry;
            const info = windowInfo(w);
            info.fraction = {
                x: (g.x - area.x) / area.width, y: (g.y - area.y) / area.height,
                width: g.width / area.width, height: g.height / area.height,
            };
            return info;
        }),
    };
}

function applyGeometry(w, r, how) {
    const before = rectStr(w.frameGeometry);
    w.frameGeometry = { x: r.x, y: r.y, width: r.width, height: r.height };
    log("PLACE(" + how + ") " + w.internalId + " " + w.desktopFileName + " before=" + before
        + " asked=" + rectStr(r) + " now=" + rectStr(w.frameGeometry));
    // Look again later: does the app fight the size?
    [50, 250, 1000, 3000].forEach(function(delay) {
        const t = new QTimer();
        t.singleShot = true;
        t.interval = delay;
        t.timeout.connect(function() {
            if (w.deleted) {
                return;
            }
            const now = rectStr(w.frameGeometry);
            log("CHECK +" + delay + "ms " + w.internalId + " " + now + (now === rectStr(r) ? " OK" : " DRIFTED"));
        });
        t.start();
    });
}

function onWindowAdded(w) {
    const info = windowInfo(w);
    log("ADDED " + JSON.stringify(info));
    w.frameGeometryChanged.connect(function() {
        log("GEOM " + w.internalId + " " + rectStr(w.frameGeometry));
    });
    w.desktopFileNameChanged.connect(function() {
        log("DFN-CHANGED " + w.internalId + " " + w.desktopFileName);
    });
    w.windowClassChanged.connect(function() {
        log("CLASS-CHANGED " + w.internalId + " " + w.resourceClass);
    });
    if (!isAppWindow(w)) {
        return;
    }
    // Synchronous path: placement already in the script.
    for (let i = 0; i < prefetched.length; i++) {
        const p = prefetched[i];
        if (p.appId === w.desktopFileName || p.appId === w.resourceClass) {
            prefetched.splice(i, 1);
            applyGeometry(w, p.rect, "sync");
            report({ event: "placed", mode: "sync", window: windowInfo(w) });
            return;
        }
    }
    // Async path: ask the daemon.
    const asked = Date.now();
    callDBus(SVC, PATH, IFACE, "ClaimWindow", JSON.stringify(info), function(reply) {
        if (!reply) {
            return;
        }
        if (w.deleted) {
            log("window gone before reply");
            return;
        }
        applyGeometry(w, JSON.parse(reply), "async rtt=" + (Date.now() - asked) + "ms");
        report({ event: "placed", mode: "async", window: windowInfo(w) });
    });
}

function handle(cmd) {
    switch (cmd.cmd) {
    case "check":
        report({ cmd: "check", result: checkEmpty() });
        break;
    case "area":
        report({ cmd: "area", result: usableArea() });
        break;
    case "list":
        report({ cmd: "list", result: listWindows() });
        break;
    case "prefetch":
        prefetched = prefetched.concat(cmd.items);
        report({ cmd: "prefetch", count: prefetched.length });
        break;
    default:
        report({ cmd: cmd.cmd, error: "unknown command" });
    }
}

function fetchWork() {
    const asked = Date.now();
    callDBus(SVC, PATH, IFACE, "TakeWork", function(json) {
        log("TakeWork rtt=" + (Date.now() - asked) + "ms");
        JSON.parse(json).forEach(handle);
    });
}

registerShortcut("VDW S3 Poke", "VDW S3: fetch work from daemon", "", fetchWork);
workspace.windowAdded.connect(onWindowAdded);
workspace.windowRemoved.connect(function(w) {
    log("REMOVED " + w.internalId + " " + w.desktopFileName);
});
log("placer loaded; " + JSON.stringify(checkEmpty()));
fetchWork();
