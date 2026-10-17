// Close every normal window in the nested session (between S4 cases).
for (const w of workspace.windowList()) {
    if (w.normalWindow) {
        print("s4close pid=" + w.pid + " class=" + w.resourceClass + " caption=\"" + w.caption + "\"");
        w.closeWindow();
    }
}
