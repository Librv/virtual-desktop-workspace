// One-shot: close every window of one app (edit APP), so the app saves its
// window state the way it would on a normal quit.
const APP = "org.kde.konsole";
let n = 0;
workspace.windowList().forEach(function(w) {
    if (w.desktopFileName === APP) {
        w.closeWindow();
        n++;
    }
});
callDBus("org.kde.vdw.S3", "/S3", "org.kde.vdw.S3", "Log", "close-app " + APP + " closed " + n);
