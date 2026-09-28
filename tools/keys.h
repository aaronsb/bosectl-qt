/*
    SPDX-FileCopyrightText: 2026 Aaron Bockelie <aaronsb@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
/* Key name to evdev keycode table for fakeinput.
 * Codes are from linux/input-event-codes.h. */
#ifndef FAKEINPUT_KEYS_H
#define FAKEINPUT_KEYS_H

#include <string.h>

struct keyname {
    const char *name;
    unsigned code;
};

static const struct keyname KEY_TABLE[] = {
    { "esc", 1 },
    { "1", 2 }, { "2", 3 }, { "3", 4 }, { "4", 5 }, { "5", 6 },
    { "6", 7 }, { "7", 8 }, { "8", 9 }, { "9", 10 }, { "0", 11 },
    { "minus", 12 },
    { "equal", 13 }, { "plus", 13 },
    { "tab", 15 },
    { "q", 16 }, { "w", 17 }, { "e", 18 }, { "r", 19 }, { "t", 20 },
    { "y", 21 }, { "u", 22 }, { "i", 23 }, { "o", 24 }, { "p", 25 },
    { "enter", 28 },
    { "ctrl", 29 }, { "leftctrl", 29 },
    { "a", 30 }, { "s", 31 }, { "d", 32 }, { "f", 33 }, { "g", 34 },
    { "h", 35 }, { "j", 36 }, { "k", 37 }, { "l", 38 },
    { "shift", 42 }, { "leftshift", 42 },
    { "z", 44 }, { "x", 45 }, { "c", 46 }, { "v", 47 }, { "b", 48 },
    { "n", 49 }, { "m", 50 },
    { "alt", 56 }, { "leftalt", 56 },
    { "space", 57 },
    { "f1", 59 }, { "f2", 60 }, { "f3", 61 }, { "f4", 62 }, { "f5", 63 },
    { "f6", 64 }, { "f7", 65 }, { "f8", 66 }, { "f9", 67 }, { "f10", 68 },
    { "f11", 87 }, { "f12", 88 },
    { "home", 102 },
    { "up", 103 },
    { "left", 105 },
    { "right", 106 },
    { "end", 107 },
    { "down", 108 },
    { "delete", 111 },
    { "meta", 125 }, { "leftmeta", 125 },
};

/* Returns the evdev code for a key name, or 0 if the name is unknown. */
static inline unsigned key_lookup(const char *name)
{
    for (size_t i = 0; i < sizeof KEY_TABLE / sizeof KEY_TABLE[0]; i++)
        if (strcmp(KEY_TABLE[i].name, name) == 0)
            return KEY_TABLE[i].code;
    return 0;
}

#endif
