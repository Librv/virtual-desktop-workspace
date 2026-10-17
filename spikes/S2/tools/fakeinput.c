// SPDX-License-Identifier: GPL-2.0-or-later
// S2 spike: drive KWin's org_kde_kwin_fake_input from the command line.
// ONLY for the nested dev session: KWin only offers the global to clients
// whose executable is listed in a .desktop file with
// X-KDE-Wayland-Interfaces=org_kde_kwin_fake_input, and the nested session
// has its own XDG_DATA_HOME for that.
//
// Usage: s2-fakeinput CMD...
//   abs X Y          move pointer to absolute position
//   rel DX DY        move pointer by a delta
//   key CODE down|up evdev key code (29 LEFTCTRL, 125 LEFTMETA, 32 D, 1 ESC)
//   btn CODE down|up evdev button code (272 BTN_LEFT)
//   sleep MS
#define _POSIX_C_SOURCE 200809L
#include "fake-input-client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-client.h>

static struct org_kde_kwin_fake_input *fake;

static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    (void)data;
    if (strcmp(interface, org_kde_kwin_fake_input_interface.name) == 0) {
        uint32_t v = version < 4 ? version : 4;
        fake = wl_registry_bind(registry, name, &org_kde_kwin_fake_input_interface, v);
        fprintf(stderr, "s2-fakeinput: bound org_kde_kwin_fake_input v%u (server v%u)\n", v, version);
    }
}

static void global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {global, global_remove};

static void sleep_ms(long ms)
{
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

static uint32_t parse_state(const char *s)
{
    return strcmp(s, "down") == 0 || strcmp(s, "1") == 0 ? 1 : 0;
}

int main(int argc, char **argv)
{
    struct wl_display *display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "s2-fakeinput: cannot connect to WAYLAND_DISPLAY\n");
        return 1;
    }
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);
    if (!fake) {
        fprintf(stderr, "s2-fakeinput: org_kde_kwin_fake_input not offered (not authorized?)\n");
        return 2;
    }
    org_kde_kwin_fake_input_authenticate(fake, "vdw-s2-fakeinput", "S2 spike nested-session test");
    wl_display_roundtrip(display);

    for (int i = 1; i < argc; ++i) {
        const char *cmd = argv[i];
        if (strcmp(cmd, "abs") == 0 && i + 2 < argc) {
            double x = atof(argv[++i]), y = atof(argv[++i]);
            org_kde_kwin_fake_input_pointer_motion_absolute(fake, wl_fixed_from_double(x), wl_fixed_from_double(y));
        } else if (strcmp(cmd, "rel") == 0 && i + 2 < argc) {
            double x = atof(argv[++i]), y = atof(argv[++i]);
            org_kde_kwin_fake_input_pointer_motion(fake, wl_fixed_from_double(x), wl_fixed_from_double(y));
        } else if (strcmp(cmd, "key") == 0 && i + 2 < argc) {
            uint32_t code = (uint32_t)atoi(argv[++i]);
            org_kde_kwin_fake_input_keyboard_key(fake, code, parse_state(argv[++i]));
        } else if (strcmp(cmd, "btn") == 0 && i + 2 < argc) {
            uint32_t code = (uint32_t)atoi(argv[++i]);
            org_kde_kwin_fake_input_button(fake, code, parse_state(argv[++i]));
        } else if (strcmp(cmd, "sleep") == 0 && i + 1 < argc) {
            sleep_ms(atol(argv[++i]));
        } else {
            fprintf(stderr, "s2-fakeinput: bad command at '%s'\n", cmd);
            return 3;
        }
        if (wl_display_roundtrip(display) < 0) {
            fprintf(stderr, "s2-fakeinput: connection error %d\n", wl_display_get_error(display));
            return 4;
        }
    }
    wl_display_disconnect(display);
    return 0;
}
