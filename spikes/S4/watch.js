// Watch windows appear while launches run. Load with a long VDW_SCRIPT_WAIT
// (dev-session.sh unloads the script after that), then launch.
// Prints "s4win <ms-since-epoch> <event> ..." so times compare with s4launch.
function desc(w) {
    return "id=" + w.internalId + " pid=" + w.pid + " class=" + w.resourceClass
        + " name=" + w.resourceName + " dfn=" + w.desktopFileName
        + " normal=" + w.normalWindow + " transient=" + w.transient
        + " tag=" + w.tag + " caption=\"" + w.caption + "\"";
}
function track(w) {
    w.desktopFileNameChanged.connect(function () {
        print("s4win " + Date.now() + " dfnChanged " + desc(w));
    });
    w.captionChanged.connect(function () {
        print("s4win " + Date.now() + " captionChanged " + desc(w));
    });
}
for (const w of workspace.windowList()) {
    if (w.normalWindow) {
        print("s4win " + Date.now() + " existing " + desc(w));
    }
}
workspace.windowAdded.connect(function (w) {
    print("s4win " + Date.now() + " added " + desc(w));
    track(w);
});
workspace.windowRemoved.connect(function (w) {
    print("s4win " + Date.now() + " removed " + desc(w));
});
print("s4win " + Date.now() + " watching");
