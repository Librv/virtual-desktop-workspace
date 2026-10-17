// One-shot: print the type flags of every window, to see what counts as
// "normal" for the emptiness check.
workspace.windowList().forEach(function(w) {
    print("S3TYPES " + w.resourceClass + " windowType=" + w.windowType + " normalWindow=" + w.normalWindow
          + " dock=" + w.dock + " desktopWindow=" + w.desktopWindow + " specialWindow=" + w.specialWindow
          + " managed=" + w.managed + " skipTaskbar=" + w.skipTaskbar + " skipSwitcher=" + w.skipSwitcher
          + " layer=" + w.layer + " onAllDesktops=" + w.onAllDesktops + " popup=" + w.popupWindow
          + " transient=" + w.transient + " minimizable=" + w.minimizable + " moveable=" + w.moveable + " resizeable=" + w.resizeable + " closeable=" + w.closeable + " wantsInput=" + w.wantsInput + " keepAbove=" + w.keepAbove);
});
