// S2 spike: list every window KWin knows (stacking order, bottom to top),
// including layer-shell surfaces, plus the active window and cursor.
const props = ["resourceClass", "resourceName", "caption", "windowType", "layer", "normalWindow",
    "dock", "onScreenDisplay", "notification", "specialWindow", "keepAbove", "skipTaskbar",
    "skipSwitcher", "wantsInput", "acceptsFocus", "active", "pid", "outline", "opacity"];
const stack = workspace.stackingOrder;
for (let i = 0; i < stack.length; ++i) {
    const w = stack[i];
    let line = "s2win[" + i + "]";
    for (const p of props) {
        line += " " + p + "=" + w[p];
    }
    const g = w.frameGeometry;
    line += " geometry=" + g.x + "," + g.y + " " + g.width + "x" + g.height;
    print(line);
}
const a = workspace.activeWindow;
print("s2active " + (a ? a.resourceClass + " '" + a.caption + "'" : "none"));
print("s2cursor " + workspace.cursorPos.x + "," + workspace.cursorPos.y);
print("s2done count=" + stack.length);
