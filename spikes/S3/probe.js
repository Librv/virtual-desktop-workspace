// One-shot probe of the scripting API available in KWin 6.7.
print("typeof callDBus=" + typeof callDBus + " QTimer=" + typeof QTimer
      + " Qt=" + typeof Qt + " registerShortcut=" + typeof registerShortcut
      + " readConfig=" + typeof readConfig);
print("desktops=" + workspace.desktops.length + " current=" + workspace.currentDesktop.name
      + " id=" + workspace.currentDesktop.id);
print("activeScreen=" + workspace.activeScreen.name + " screens=" + workspace.screens.length);
var a = workspace.clientArea(KWin.PlacementArea, workspace.activeScreen, workspace.currentDesktop);
print("area=" + a.x + "," + a.y + " " + a.width + "x" + a.height);
callDBus("org.kde.vdw.S3", "/S3", "org.kde.vdw.S3", "Log", "hello from probe");
callDBus("org.kde.vdw.S3", "/S3", "org.kde.vdw.S3", "TakeWork",
         function(r) { print("TakeWork reply type=" + typeof r + " value=" + r); });
try {
    var t = new QTimer();
    t.singleShot = true;
    t.interval = 100;
    t.timeout.connect(function() { print("QTimer fired"); });
    t.start();
    print("QTimer ok");
} catch (e) {
    print("QTimer err " + e);
}
print("probe end");
