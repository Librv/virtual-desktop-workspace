// Print every non-function property KWin exposes on each normal window.
for (const w of workspace.windowList()) {
    if (!w.normalWindow) continue;
    const parts = [];
    for (const k in w) {
        let v;
        try { v = w[k]; } catch (e) { v = "<err>"; }
        if (typeof v === "function") continue;
        parts.push(k + "=" + v);
    }
    print("s4prop " + parts.join(" | "));
}
print("s4prop done");
