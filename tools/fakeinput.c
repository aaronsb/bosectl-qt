/*
    SPDX-FileCopyrightText: 2026 Aaron Bockelie <aaronsb@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
/* fakeinput: inject pointer and keyboard events into a KWin Wayland session
 * through org_kde_kwin_fake_input.
 *
 * Usage:
 *   fakeinput [--display NAME] [-v]         commands from stdin, one per line
 *   fakeinput [--display NAME] [-v] CMD...  one command from argv
 *
 * Commands:
 *   move X Y                       absolute pointer position, instantly
 *   glide X Y [STEPS] [MS]         eased travel from the last position (demos)
 *   rel DX DY                      relative pointer motion
 *   down [left|middle|right]       button press (default left)
 *   up [left|middle|right]         button release
 *   click [left|middle|right]      press, ~30 ms, release
 *   drag X1 Y1 X2 Y2 [STEPS] [MS]  left-drag, eased, STEPS (20) steps MS (10) apart
 *   wheel N                        N notches; positive = scroll up
 *   key NAME [down|up]             evdev key by name or decimal code
 *   sleep MS
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include <wayland-client.h>
#include "fake-input-client-protocol.h"
#include "keys.h"

#define FAKE_INPUT_VERSION 4

#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111
#define BTN_MIDDLE 0x112

#define WHEEL_NOTCH 15.0   /* KWin: one notch on the fake axis is 15 units */
#define AXIS_VERTICAL 0

static struct wl_display *display;
static struct org_kde_kwin_fake_input *fake;
static uint32_t fake_name;
static uint32_t fake_version;
static bool verbose;

static void die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("fakeinput: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

static void msleep(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
        ;
}

static void flush(void)
{
    if (wl_display_flush(display) < 0 && errno != EAGAIN)
        die("flush failed: %s", strerror(errno));
}

/* ---- registry ---------------------------------------------------------- */

static void registry_global(void *data, struct wl_registry *reg, uint32_t name,
                            const char *iface, uint32_t version)
{
    (void)data;
    (void)reg;
    if (strcmp(iface, org_kde_kwin_fake_input_interface.name) == 0) {
        fake_name = name;
        fake_version = version;
    }
}

static void registry_global_remove(void *data, struct wl_registry *reg, uint32_t name)
{
    (void)data;
    (void)reg;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    registry_global,
    registry_global_remove,
};

static void connect_display(const char *name)
{
    display = wl_display_connect(name);
    if (!display)
        die("cannot connect to Wayland display %s", name ? name : "(default)");

    struct wl_registry *reg = wl_display_get_registry(display);
    wl_registry_add_listener(reg, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!fake_name)
        die("compositor does not advertise org_kde_kwin_fake_input\n"
            "  KWin only shows it to executables registered with a desktop file "
            "(make -C tools desktop) or when kwin_wayland runs with "
            "KWIN_WAYLAND_NO_PERMISSION_CHECKS=1");
    if (fake_version < FAKE_INPUT_VERSION)
        die("org_kde_kwin_fake_input version %u advertised, need %d",
            fake_version, FAKE_INPUT_VERSION);

    fake = wl_registry_bind(reg, fake_name, &org_kde_kwin_fake_input_interface,
                            FAKE_INPUT_VERSION);
    if (!fake)
        die("binding org_kde_kwin_fake_input failed");

    org_kde_kwin_fake_input_authenticate(fake, "kwin-canvas-tests", "automated testing");
    if (wl_display_roundtrip(display) < 0)
        die("authentication could not be sent: %s", strerror(errno));
}

/* ---- primitives -------------------------------------------------------- */

static void ptr_abs(double x, double y)
{
    org_kde_kwin_fake_input_pointer_motion_absolute(fake, wl_fixed_from_double(x),
                                                    wl_fixed_from_double(y));
}

static void ptr_rel(double dx, double dy)
{
    org_kde_kwin_fake_input_pointer_motion(fake, wl_fixed_from_double(dx),
                                           wl_fixed_from_double(dy));
}

static void button(uint32_t btn, uint32_t state)
{
    org_kde_kwin_fake_input_button(fake, btn, state);
}

static void key(uint32_t code, uint32_t state)
{
    org_kde_kwin_fake_input_keyboard_key(fake, code, state);
}

static void axis(double value)
{
    org_kde_kwin_fake_input_axis(fake, AXIS_VERTICAL, wl_fixed_from_double(value));
}

/* ---- command parsing --------------------------------------------------- */

static bool parse_double(const char *s, double *out)
{
    char *end;
    if (!s || !*s)
        return false;
    *out = strtod(s, &end);
    return *end == '\0';
}

static bool parse_long(const char *s, long *out)
{
    char *end;
    if (!s || !*s)
        return false;
    *out = strtol(s, &end, 10);
    return *end == '\0';
}

static bool parse_button(const char *s, uint32_t *out)
{
    if (!s || strcmp(s, "left") == 0) {
        *out = BTN_LEFT;
    } else if (strcmp(s, "right") == 0) {
        *out = BTN_RIGHT;
    } else if (strcmp(s, "middle") == 0) {
        *out = BTN_MIDDLE;
    } else {
        return false;
    }
    return true;
}

static bool parse_key(const char *s, uint32_t *out)
{
    long n;
    if (!s)
        return false;
    if (isdigit((unsigned char)s[0]) && parse_long(s, &n) && n > 0 && n < 0x300) {
        *out = (uint32_t)n;
        return true;
    }
    unsigned code = key_lookup(s);
    if (!code)
        return false;
    *out = code;
    return true;
}

/* Last absolute position sent, so glide has somewhere to start from. */
static double cur_x = -1, cur_y = -1;
static bool have_cur = false;

static double ease(double t)
{
    return t * t * (3.0 - 2.0 * t);   /* smoothstep */
}

/* Eased travel from (x1,y1) to (x2,y2) over STEPS steps, MS apart. */
static void travel(double x1, double y1, double x2, double y2, long steps, long ms)
{
    for (long i = 1; i <= steps; i++) {
        double t = ease((double)i / (double)steps);
        ptr_abs(x1 + (x2 - x1) * t, y1 + (y2 - y1) * t);
        cur_x = x1 + (x2 - x1) * t;
        cur_y = y1 + (y2 - y1) * t;
        have_cur = true;
        flush();
        msleep(ms);
    }
}

#define ARG(i) ((i) < argc ? argv[i] : NULL)

/* Runs one command. Returns false on a usage error (message already printed). */
static bool run_command(int argc, char **argv)
{
    if (argc == 0)
        return true;
    const char *cmd = argv[0];

    if (verbose) {
        fputs("fakeinput:", stderr);
        for (int i = 0; i < argc; i++)
            fprintf(stderr, " %s", argv[i]);
        fputc('\n', stderr);
    }

    if (strcmp(cmd, "move") == 0) {
        double x, y;
        if (argc != 3 || !parse_double(argv[1], &x) || !parse_double(argv[2], &y))
            goto usage;
        ptr_abs(x, y);
        cur_x = x; cur_y = y; have_cur = true;
    } else if (strcmp(cmd, "glide") == 0) {
        double x, y;
        long steps = 30, ms = 12;
        if (argc < 3 || argc > 5 || !parse_double(argv[1], &x) || !parse_double(argv[2], &y))
            goto usage;
        if (argc >= 4 && (!parse_long(argv[3], &steps) || steps < 1))
            goto usage;
        if (argc >= 5 && (!parse_long(argv[4], &ms) || ms < 0))
            goto usage;
        if (!have_cur) {
            ptr_abs(x, y);
            cur_x = x; cur_y = y; have_cur = true;
        } else {
            travel(cur_x, cur_y, x, y, steps, ms);
        }
    } else if (strcmp(cmd, "rel") == 0) {
        double dx, dy;
        if (argc != 3 || !parse_double(argv[1], &dx) || !parse_double(argv[2], &dy))
            goto usage;
        ptr_rel(dx, dy);
    } else if (strcmp(cmd, "down") == 0 || strcmp(cmd, "up") == 0) {
        uint32_t btn;
        if (argc > 2 || !parse_button(ARG(1), &btn))
            goto usage;
        button(btn, strcmp(cmd, "down") == 0 ? 1 : 0);
    } else if (strcmp(cmd, "click") == 0) {
        uint32_t btn;
        if (argc > 2 || !parse_button(ARG(1), &btn))
            goto usage;
        button(btn, 1);
        flush();
        msleep(30);
        button(btn, 0);
    } else if (strcmp(cmd, "drag") == 0) {
        double x1, y1, x2, y2;
        long steps = 20, ms = 10;
        if (argc < 5 || argc > 7 || !parse_double(argv[1], &x1) || !parse_double(argv[2], &y1)
            || !parse_double(argv[3], &x2) || !parse_double(argv[4], &y2))
            goto usage;
        if (argc >= 6 && (!parse_long(argv[5], &steps) || steps < 1))
            goto usage;
        if (argc >= 7 && (!parse_long(argv[6], &ms) || ms < 0))
            goto usage;
        ptr_abs(x1, y1);
        cur_x = x1; cur_y = y1; have_cur = true;
        flush();
        msleep(ms);
        button(BTN_LEFT, 1);
        flush();
        msleep(ms);
        travel(x1, y1, x2, y2, steps, ms);
        button(BTN_LEFT, 0);
    } else if (strcmp(cmd, "wheel") == 0) {
        long n;
        if (argc != 2 || !parse_long(argv[1], &n))
            goto usage;
        /* Positive N scrolls up, and KWin's fake axis maps -15 to scroll up. */
        double value = n > 0 ? -WHEEL_NOTCH : WHEEL_NOTCH;
        long count = n < 0 ? -n : n;
        for (long i = 0; i < count; i++) {
            if (i > 0)
                msleep(20);
            axis(value);
            flush();
        }
    } else if (strcmp(cmd, "key") == 0) {
        uint32_t code;
        if (argc < 2 || argc > 3 || !parse_key(argv[1], &code))
            goto usage;
        if (argc == 3) {
            if (strcmp(argv[2], "down") == 0) {
                key(code, 1);
            } else if (strcmp(argv[2], "up") == 0) {
                key(code, 0);
            } else {
                goto usage;
            }
        } else {
            key(code, 1);
            flush();
            msleep(20);
            key(code, 0);
        }
    } else if (strcmp(cmd, "sleep") == 0) {
        long ms;
        if (argc != 2 || !parse_long(argv[1], &ms) || ms < 0)
            goto usage;
        flush();
        msleep(ms);
    } else {
        fprintf(stderr, "fakeinput: unknown command '%s'\n", cmd);
        return false;
    }

    flush();
    return true;

usage:
    fprintf(stderr, "fakeinput: bad arguments for '%s'\n", cmd);
    return false;
}

/* Splits a line on whitespace in place. Returns the token count. */
static int tokenize(char *line, char **argv, int max)
{
    int n = 0;
    char *save;
    for (char *tok = strtok_r(line, " \t\r\n", &save); tok && n < max;
         tok = strtok_r(NULL, " \t\r\n", &save))
        argv[n++] = tok;
    return n;
}

static void usage_exit(void)
{
    fputs("usage: fakeinput [--display NAME] [-v] [CMD ARGS...]\n"
          "  glide X Y [STEPS] [MS]        eased pointer travel from the last position\n"
          "commands: move X Y | rel DX DY | down|up|click [left|middle|right]\n"
          "          drag X1 Y1 X2 Y2 [STEPS] [MS] | wheel N | key NAME [down|up] | sleep MS\n",
          stderr);
    exit(2);
}

int main(int argc, char **argv)
{
    const char *display_name = NULL;
    int i = 1;

    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "--display") == 0) {
            if (i + 1 >= argc)
                usage_exit();
            display_name = argv[i + 1];
            i += 2;
        } else if (strncmp(argv[i], "--display=", 10) == 0) {
            display_name = argv[i] + 10;
            i++;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
            i++;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage_exit();
        } else if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        } else {
            fprintf(stderr, "fakeinput: unknown option '%s'\n", argv[i]);
            usage_exit();
        }
    }

    connect_display(display_name);

    int status = 0;
    if (i < argc) {
        if (!run_command(argc - i, argv + i))
            status = 2;
    } else {
        char *line = NULL;
        size_t cap = 0;
        ssize_t len;
        while ((len = getline(&line, &cap, stdin)) != -1) {
            char *hash = strchr(line, '#');
            if (hash)
                *hash = '\0';
            char *toks[16];
            int n = tokenize(line, toks, 16);
            if (n == 0)
                continue;
            if (!run_command(n, toks)) {
                status = 2;
                break;
            }
        }
        free(line);
    }

    if (wl_display_roundtrip(display) < 0) {
        fprintf(stderr, "fakeinput: final roundtrip failed: %s\n", strerror(errno));
        status = status ? status : 1;
    }
    /* The destroy request exists from version 5; at version 4 drop the proxy locally. */
    wl_proxy_destroy((struct wl_proxy *)fake);
    wl_display_disconnect(display);
    return status;
}
