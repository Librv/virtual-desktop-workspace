// One-shot query: is the current desktop free of app windows? vdwd replaces
// the @...@ placeholders before loading, and unloads the script afterwards.
(function() {
    const ownPid = @OWN_PID@;
    const desktop = workspace.currentDesktop;
    const count = workspace.windowList().filter(function(w) {
        return w.normalWindow && w.minimizable && w.pid !== ownPid
            && (w.onAllDesktops || w.desktops.indexOf(desktop) !== -1);
    }).length;
    callDBus("io.github.librv.vdw", "/Script", "io.github.librv.vdw.Script", "QueryResult",
             "@REQUEST_ID@", JSON.stringify({ empty: count === 0, count: count }));
})();
