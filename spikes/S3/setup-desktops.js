// One-shot test fixture: add a second desktop, move Kate there, minimize
// Dolphin, so check/list can be tested against desktop membership.
if (workspace.desktops.length < 2) {
    workspace.createDesktop(1, "Desktop 2");
}
const d2 = workspace.desktops[1];
workspace.windowList().forEach(function(w) {
    if (w.desktopFileName === "org.kde.kate") {
        w.desktops = [d2];
    }
    if (w.desktopFileName === "org.kde.dolphin") {
        w.minimized = true;
    }
});
callDBus("org.kde.vdw.S3", "/S3", "org.kde.vdw.S3", "Log",
         "setup-desktops: desktops=" + workspace.desktops.map(function(d) { return d.name; }).join("|")
         + " current=" + workspace.currentDesktop.name);
