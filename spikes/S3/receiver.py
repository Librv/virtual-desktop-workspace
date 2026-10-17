#!/usr/bin/env python3
# Throwaway D-Bus receiver for spike S3. Stands in for vdwd on the NESTED bus.
# Run only through: scripts/dev-session.sh run -- python3 spikes/S3/receiver.py LOG
#
# Interface org.kde.vdw.S3 at /S3 on name org.kde.vdw.S3:
#   Log(s)                     script -> receiver: free-form log line
#   Report(s json)             script -> receiver: structured result
#   ClaimWindow(s json) -> s   script asks "is there a pending placement for
#                              this new window?"; returns a rect as JSON or ""
#   TakeWork() -> s            script fetches queued commands (JSON array)
#   AddPending(s appId, i x, i y, i w, i h)   driver: queue a placement
#   QueueWork(s json)          driver: queue a command for TakeWork
#   Pending() -> s             driver: what is still unclaimed
import json
import sys
import time

from gi.repository import Gio, GLib

XML = """
<node>
  <interface name="org.kde.vdw.S3">
    <method name="Log"><arg type="s" direction="in"/></method>
    <method name="Report"><arg type="s" direction="in"/></method>
    <method name="ClaimWindow">
      <arg type="s" direction="in"/><arg type="s" direction="out"/>
    </method>
    <method name="TakeWork"><arg type="s" direction="out"/></method>
    <method name="AddPending">
      <arg type="s" direction="in"/><arg type="i" direction="in"/>
      <arg type="i" direction="in"/><arg type="i" direction="in"/>
      <arg type="i" direction="in"/>
    </method>
    <method name="QueueWork"><arg type="s" direction="in"/></method>
    <method name="Pending"><arg type="s" direction="out"/></method>
  </interface>
</node>
"""

log_path = sys.argv[1] if len(sys.argv) > 1 else "/dev/stdout"
log_file = open(log_path, "a", buffering=1)
t0 = time.monotonic()
pending = []  # [{appId, rect, queuedAt}]
work = []


def log(msg):
    log_file.write("%9.3f %s\n" % (time.monotonic() - t0, msg))


def on_call(conn, sender, path, iface, method, params, inv):
    args = params.unpack()
    if method == "Log":
        log("LOG " + args[0])
        inv.return_value(None)
    elif method == "Report":
        log("REPORT " + args[0])
        inv.return_value(None)
    elif method == "ClaimWindow":
        info = json.loads(args[0])
        reply = ""
        for i, p in enumerate(pending):
            if p["appId"] in (info.get("desktopFileName"), info.get("resourceClass")):
                del pending[i]
                age = (time.monotonic() - p["queuedAt"]) * 1000
                reply = json.dumps(p["rect"])
                log("CLAIM %s -> %s (pending for %.0f ms)" % (info.get("id"), reply, age))
                break
        else:
            log("CLAIM %s (%s) -> none" % (info.get("id"), info.get("desktopFileName")))
        inv.return_value(GLib.Variant("(s)", (reply,)))
    elif method == "TakeWork":
        out = json.dumps(work)
        work.clear()
        log("TAKEWORK -> " + out)
        inv.return_value(GLib.Variant("(s)", (out,)))
    elif method == "AddPending":
        app, x, y, w, h = args
        pending.append({"appId": app, "rect": {"x": x, "y": y, "width": w, "height": h},
                        "queuedAt": time.monotonic()})
        log("ADDPENDING %s %d,%d %dx%d" % (app, x, y, w, h))
        inv.return_value(None)
    elif method == "QueueWork":
        work.append(json.loads(args[0]))
        log("QUEUEWORK " + args[0])
        inv.return_value(None)
    elif method == "Pending":
        inv.return_value(GLib.Variant("(s)", (json.dumps(pending),)))


bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
node = Gio.DBusNodeInfo.new_for_xml(XML)
bus.register_object("/S3", node.interfaces[0], on_call, None, None)
Gio.bus_own_name_on_connection(bus, "org.kde.vdw.S3", Gio.BusNameOwnerFlags.NONE,
                               lambda *a: log("name acquired"), lambda *a: log("name lost"))
log("receiver up on " + bus.get_unique_name())
GLib.MainLoop().run()
