// One-shot alternative to the doorbell: a tiny script loaded per request
// that answers one query (usable area + emptiness) and reports it.
const d = workspace.currentDesktop;
const a = workspace.clientArea(KWin.PlacementArea, workspace.activeScreen, d);
const n = workspace.windowList().filter(function(w) {
    return w.normalWindow && w.minimizable && (w.onAllDesktops || w.desktops.indexOf(d) !== -1);
}).length;
callDBus("org.kde.vdw.S3", "/S3", "org.kde.vdw.S3", "Report",
         JSON.stringify({ oneshot: true, blockers: n, area: [a.x, a.y, a.width, a.height] }));
