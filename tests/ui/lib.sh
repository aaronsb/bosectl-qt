#!/usr/bin/env bash
# Test library for the nested UI scenarios, after kwin-canvas's tests/lib.sh.
# Sourced by tests/ui/run.sh and docs/screenshots.sh; scenarios are sourced
# after it and define run_scenario(). Reuses dev/nest.sh for everything that
# talks to the nest.

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
export NEST_NAME=${NEST_NAME:-test}
# Offscreen: a nest that is a window on the live desktop slows down when it
# is covered, and every timing in the scenarios goes with it.
export NEST_VIRTUAL=${NEST_VIRTUAL:-1}
# shellcheck source=../../dev/nest.sh
source "$HERE/dev/nest.sh"
# nest.sh sets -e for its own use; a test runner must survive failing checks.
set +e

OUT=${TEST_OUT:-$HERE/build/test-ui}
GOLDEN_DIR=$HERE/tests/ui/golden
UPDATE_GOLDEN=${UPDATE_GOLDEN:-0}
TOLERANCE=${GOLDEN_TOLERANCE:-0.02}
PASS=0; FAIL=0; SKIP=0
mkdir -p "$OUT"

# ---- assertions -------------------------------------------------------------
ok()   { PASS=$((PASS + 1)); echo "    ok   $1"; }
fail() { FAIL=$((FAIL + 1)); echo "    FAIL $1"; }
skip() { SKIP=$((SKIP + 1)); echo "    skip $1"; }

assert_eq() {   # name actual expected
    if [ "$2" = "$3" ]; then ok "$1 = $3"; else fail "$1: expected '$3', got '$2'"; fi
}
assert_true() { # name shell-test-args...
    local name=$1; shift
    if test "$@"; then ok "$name"; else fail "$name"; fi
}
# assert_until NAME CMD...: CMD succeeds within ~5 s.
assert_until() {
    local name=$1; shift
    for _ in $(seq 1 25); do
        if "$@" >/dev/null 2>&1; then ok "$name"; return 0; fi
        sleep 0.2
    done
    fail "$name"
}

# The simulated headset's state line has key=value fields; dev FIELD reads one.
dev() { sim Device | tr ' ' '\n' | sed -n "s/^$1=//p"; }
dev_is() { [ "$(dev "$1")" = "$2" ]; }
menu_has() { sim Menu | grep -qxF -- "$1"; }
notifications_open() { [ -n "$(notifications)" ]; }
# widgets_match "Caption" REGEX: a line of `sim Widgets` matches.
widgets_match() { sim Widgets "$1" | grep -q -- "$2"; }

# ---- the tray menu ----------------------------------------------------------
# Open the tray menu with a real right-click, as a user would. Plasma draws
# it from the app's exported dbusmenu.
menu_open() {
    local x y
    read -r x y < <(tray) || return 1
    click "$x" "$y" right
    for _ in $(seq 1 25); do [ -n "$(popups)" ] && return 0; sleep 0.2; done
    return 1
}
menu_close() {
    for _ in 1 2 3; do [ -z "$(popups)" ] && return 0; input key esc; sleep 0.3; done
    [ -z "$(popups)" ]
}
# Centre of a top-level menu entry, "X Y", from the open menu's geometry and
# the entry's place in `sim Menu`. Plasma's rows are 29 px, separators add
# 5 px, and the first row's centre is 18 px below the popup's top edge.
menu_entry() {
    local label=$1 mx my mw mh
    read -r mx my mw mh < <(popups | head -1) || return 1
    local rows=0 seps=0 line
    while IFS= read -r line; do
        [[ "$line" == " "* ]] && continue
        if [ "$line" = "-" ]; then seps=$((seps + 1)); continue; fi
        # Match the text alone; "[checked]" and "[disabled]" are annotations.
        [ "${line%% \[*}" = "$label" ] && { echo "$((mx + mw / 2)) $((my + 18 + 29 * rows + 5 * seps))"; return 0; }
        rows=$((rows + 1))
    done < <(sim Menu)
    return 1
}
# The box around every open menu, "X Y W H", for a screenshot of a menu
# and its submenus.
menus_box() {
    popups | awk 'NR == 1 { x0 = $1; y0 = $2; x1 = $1 + $3; y1 = $2 + $4 }
        { if ($1 < x0) x0 = $1; if ($2 < y0) y0 = $2
          if ($1 + $3 > x1) x1 = $1 + $3; if ($2 + $4 > y1) y1 = $2 + $4 }
        END { if (NR) print x0, y0, x1 - x0, y1 - y0 }'
}
# Click a top-level entry of the open menu.
menu_click() {
    local x y
    read -r x y < <(menu_entry "$1") || { fail "no menu entry '$1'"; return 1; }
    click "$x" "$y"
}

# ---- windows ----------------------------------------------------------------
# Open a window from the menu through the app's own action (sim Trigger) and
# wait for KWin to map it.
open_window() {   # open_window "Menu Entry..." "Window caption"
    sim Trigger "$1" >/dev/null
    for _ in $(seq 1 25); do [ -n "$(wingeom "$2")" ] && { sleep 0.3; return 0; }; sleep 0.2; done
    return 1
}
# Centre of a widget in a window, in nest pixels: sim Widgets gives its place
# in the client area, KWin the client area's place on screen.
# widget_at "Caption" CLASS [TEXT]
widget_at() {
    local wx wy _w _h
    read -r wx wy _w _h < <(wingeom "$1") || return 1
    local line
    line=$(sim Widgets "$1" | awk -v c="$2" -v t="${3:-}" '$1 == c && (t == "" || index($0, "[" t "]")) { print; exit }')
    [ -n "$line" ] || return 1
    local x y w h
    read -r _ x y w h _ <<<"$line"
    echo "$((wx + x + w / 2)) $((wy + y + h / 2))"
}
click_widget() {   # click_widget "Caption" CLASS [TEXT]
    local x y
    read -r x y < <(widget_at "$@") || { fail "no $2 ${3:-} in '$1'"; return 1; }
    click "$x" "$y"
}

# ---- screenshots ------------------------------------------------------------
# snap NAME [X Y W H]: screenshot the nest (or a region of it); with
# UPDATE_GOLDEN=1 store it as the golden, otherwise compare against the
# golden by normalized RMSE within TOLERANCE.
snap() {
    local name=$1 file=$OUT/$1.png golden=$GOLDEN_DIR/$1.png
    # Park the pointer in the top-left corner, where it hovers nothing.
    input move 2 2
    sleep "${SNAP_SETTLE:-0.4}"
    shot "$file" >/dev/null || { fail "snap $name: screenshot failed"; return; }
    if [ $# -ge 5 ]; then
        magick "$file" -crop "${4}x${5}+${2}+${3}" +repage "$file"
    fi
    if [ "$UPDATE_GOLDEN" = 1 ]; then
        mkdir -p "$GOLDEN_DIR"; cp "$file" "$golden"; ok "golden $name updated"; return
    fi
    [ -f "$golden" ] || { skip "snap $name: no golden (make golden)"; return; }
    local metric
    # compare exits 1 whenever the images differ at all; the number is what matters.
    metric=$( { magick compare -metric RMSE "$golden" "$file" "$OUT/$name.diff.png" 2>&1 || true; } | sed -n 's/.*(\([0-9.e-]*\)).*/\1/p')
    if [ -z "$metric" ]; then fail "snap $name: compare failed (size changed?)"; return; fi
    if awk -v m="$metric" -v t="$TOLERANCE" 'BEGIN{exit !(m<=t)}'; then
        ok "snap $name (rmse $metric)"; rm -f "$OUT/$name.diff.png"
    else
        fail "snap $name: rmse $metric > $TOLERANCE, diff at $OUT/$name.diff.png"
    fi
}

# Put the app back to the state every scenario starts from: no windows or
# menus open, the headset reachable, connected and in its starting state,
# the pointer parked.
reset_app() {
    menu_close >/dev/null
    sim Close "" >/dev/null
    sim SetLatency 0 >/dev/null
    sim SetReachable true >/dev/null
    menu_has "Reconnect" || { sim Trigger Connect >/dev/null; sleep 0.5; }
    sim Reset >/dev/null
    input move 640 300
    sleep 0.3
}
