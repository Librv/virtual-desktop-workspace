// One-shot query: usable area (panels excluded) of the active screen on the
// current desktop. vdwd replaces the @...@ placeholders before loading.
(function() {
    const area = workspace.clientArea(KWin.PlacementArea, workspace.activeScreen, workspace.currentDesktop);
    callDBus("io.github.librv.vdw", "/Script", "io.github.librv.vdw.Script", "QueryResult",
             "@REQUEST_ID@", JSON.stringify({ x: Math.round(area.x), y: Math.round(area.y), width: Math.round(area.width), height: Math.round(area.height) }));
})();
