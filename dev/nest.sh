#!/usr/bin/env bash
# Nested Plasma session for bosectl-qt, adapted from kwin-canvas's harness.
#
# Runs a second kwin_wayland on its own D-Bus session bus and XDG homes, with
# plasmashell inside showing one panel that holds only a system tray. The tray
# app runs there against the simulated headset (BOSECTL_QT_SIM), so the menu,
# the windows and the notifications can be driven and screenshotted without a
# headset and without touching the real desktop or ~/.config.
#
#   dev/nest.sh up            start nested KWin + plasmashell (the tray panel)
#   dev/nest.sh app           start build/bosectl-qt against the simulated headset
#   dev/nest.sh sim METHOD..  call org.bosectl.qt /Sim (Menu, Trigger PATH, Widgets TITLE, ...)
#   dev/nest.sh shot [file]   screenshot the nest
#   dev/nest.sh input CMDS    inject pointer/keyboard events (tools/fakeinput.c)
#   dev/nest.sh js 'CODE'     run a KWin script in the nest; its out() lines come back
#   dev/nest.sh tray          print the tray icon's centre, "X Y", in nest pixels
#   dev/nest.sh click X Y [right]  a real click, held long enough for Plasma
#   dev/nest.sh popups        open menus' geometry, "X Y W H" per line
#   dev/nest.sh log [N]       tail the nest's log (KWin, plasmashell, the app)
#   dev/nest.sh down          stop it
#   dev/nest.sh clean         kill leftovers of nests whose state is gone
#
# NEST_NAME=foo runs a second, independent nest (own socket, bus, homes, log).
# NEST_VIRTUAL=1 renders to KWin's virtual framebuffer instead of a window on
# the live desktop; tests and screenshots use it.
# NEST_WIDTH/NEST_HEIGHT size the output (default 1280x800).
# NEST_SCHEME=BreezeDark picks the colour scheme (default BreezeLight).
# The file is sourceable: `source dev/nest.sh` exposes every function without
# running a command, which is how tests/ui/lib.sh reuses it.

set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
NEST_NAME=${NEST_NAME:-bosectl}
STATE=${XDG_RUNTIME_DIR:-/tmp}/bosectl-qt-nest-$NEST_NAME
CONF=$STATE/config
SOCKET=wayland-bosectl-$NEST_NAME
WIDTH=${NEST_WIDTH:-1280}
HEIGHT=${NEST_HEIGHT:-800}
VIRTUAL=${NEST_VIRTUAL:-0}
LOG=$STATE/nest.log
APP=${NEST_APP:-$HERE/build/bosectl-qt}
FAKEINPUT=$HERE/build/tools/fakeinput

bus() { cat "$STATE/bus" 2>/dev/null; }
# Everything a nest runs (compositor, bus, daemons, shell, app) runs niced and
# at idle I/O priority, so it never contends with the live session on equal
# terms.
LOW=(nice -n 10 ionice -c3)
# The nest's XDG homes: config, state, cache and data live under $STATE for
# everything in it, so no nest process reads the user's theme or writes into
# the real home. ~/.local/share stays on the data search path for icons.
nest_env() {
    printf '%s\n' "XDG_CONFIG_HOME=$CONF" "XDG_STATE_HOME=$STATE/state" "XDG_CACHE_HOME=$STATE/cache" \
        "XDG_DATA_HOME=$STATE/data" \
        "XDG_DATA_DIRS=$HOME/.local/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
}
# The nest's bus address, or a loud failure: an empty DBUS_SESSION_BUS_ADDRESS
# makes D-Bus fall back to the live session's bus.
nestbus() {
    local b; b=$(bus)
    [ -n "$b" ] || { echo "nest $NEST_NAME has no bus ($STATE/bus); not falling back to the session bus" >&2; return 1; }
    echo "$b"
}
nq() { local b; b=$(nestbus) || return 1; DBUS_SESSION_BUS_ADDRESS=$b qdbus6 "$@"; }
alive() { [ -f "$STATE/pid" ] && kill -0 "$(cat "$STATE/pid")" 2>/dev/null; }

seed_config() {
    rm -rf "$CONF" "$STATE/state" "$STATE/cache" "$STATE/data" "$STATE/desktop"
    mkdir -p "$CONF" "$STATE/state" "$STATE/cache" "$STATE/data" "$STATE/desktop"
    # No animations: a screenshot never catches a menu or window half-faded.
    # A fixed colour scheme, fonts and icons, whatever the user runs.
    cat > "$CONF/kdeglobals" <<CFG
[KDE]
AnimationDurationFactor=0
LookAndFeelPackage=org.kde.breeze.desktop

[General]
ColorScheme=${NEST_SCHEME:-BreezeLight}
font=Noto Sans,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,1
menuFont=Noto Sans,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,1
smallestReadableFont=Noto Sans,8,-1,5,400,0,0,0,0,0,0,0,0,0,0,1
toolBarFont=Noto Sans,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,1
fixed=Hack,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,1

[Icons]
Theme=breeze
CFG
    # Plasma's own surfaces (panel, notifications) take the Plasma style, not
    # the colour scheme: dark schemes get breeze-dark.
    local plasma_theme=default
    case ${NEST_SCHEME:-BreezeLight} in *Dark*) plasma_theme=breeze-dark ;; esac
    cat > "$CONF/plasmarc" <<CFG
[Theme]
name=$plasma_theme
CFG
    cat > "$CONF/kwinrc" <<CFG
[Plugins]
blurEnabled=false
contrastEnabled=false
slidingpopupsEnabled=false
fadingpopupsEnabled=false
scaleEnabled=false

[Compositing]
Backend=OpenGL

[Windows]
Placement=Centered
CFG
    # kded's GTK module writes $HOME/.gtkrc-2.0 whatever XDG_CONFIG_HOME says,
    # and the live session's kded reloads its xsettings on that.
    cat > "$CONF/kded5rc" <<CFG
[Module-gtkconfig]
autoload=false
CFG
    # The nest's bus activates every stock service but the portals. The
    # portal's background monitor reports the host's running flatpaks (it
    # reads systemd's app scopes, not the bus), and Plasma's tray shows each
    # one as an icon, so a nest would picture whatever the user has open.
    mkdir -p "$STATE/dbus/services"
    rm -f "$STATE/dbus/services"/*
    local svc
    for svc in /usr/share/dbus-1/services/*.service; do
        case ${svc##*/} in *portal*) continue ;; esac
        ln -s "$svc" "$STATE/dbus/services/"
    done
    cat > "$STATE/dbus/session.conf" <<CFG
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <keep_umask/>
  <listen>unix:tmpdir=/tmp</listen>
  <auth>EXTERNAL</auth>
  <servicedir>$STATE/dbus/services</servicedir>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
CFG
    # Popups close after 5 s: long enough to screenshot, short enough that a
    # scenario's error notification is gone before the next one starts.
    cat > "$CONF/plasmanotifyrc" <<CFG
[Notifications]
PopupTimeout=5000
CFG
}

up() {
    if alive; then echo "already up (pid $(cat "$STATE/pid"))"; return; fi
    mkdir -p "$STATE"; rm -f "$STATE/bus" "$STATE/pid" "$STATE/clients"
    seed_config
    # setsid: the nest gets its own session, so nothing that happens to the
    # shell or make that started it can reap it. The pid recorded is
    # dbus-run-session's. The bus and every daemon it activates see the nest's
    # Wayland socket and no X display; kwin_wayland alone gets the caller's
    # displays back, which a windowed nest needs.
    (
        cd "$STATE"
        setsid -f "${LOW[@]}" env -u DISPLAY WAYLAND_DISPLAY="$SOCKET" \
            NEST_HOST_WAYLAND="${WAYLAND_DISPLAY:-}" NEST_HOST_DISPLAY="${DISPLAY:-}" \
            $(nest_env) \
            QT_LOGGING_TO_CONSOLE=1 KWIN_WAYLAND_NO_PERMISSION_CHECKS=1 \
            dbus-run-session --config-file="$STATE/dbus/session.conf" -- bash -c '
            echo "$PPID" > "$0/pid"
            echo "$DBUS_SESSION_BUS_ADDRESS" > "$0/bus"
            export WAYLAND_DISPLAY="$NEST_HOST_WAYLAND"
            [ -n "$NEST_HOST_DISPLAY" ] && export DISPLAY="$NEST_HOST_DISPLAY"
            unset NEST_HOST_WAYLAND NEST_HOST_DISPLAY
            exec kwin_wayland '"$([ "$VIRTUAL" = 1 ] && echo --virtual)"' --width '"$WIDTH"' --height '"$HEIGHT"' --no-lockscreen --socket '"$SOCKET"'
        ' "$STATE" > "$LOG" 2>&1
    )
    local ok=0
    for _ in $(seq 1 50); do
        [ -f "$STATE/bus" ] && nq org.kde.KWin /KWin org.kde.KWin.supportInformation >/dev/null 2>&1 && { ok=1; break; }
        sleep 0.2
    done
    [ "$ok" = 1 ] || { echo "nested KWin did not come up; see: dev/nest.sh log"; return 1; }
    echo "nested KWin up (socket $SOCKET)"
    unrealtime
    shell
}

# /usr/bin/kwin_wayland carries cap_sys_nice and gives its threads SCHED_RR,
# the live compositor's own class. A nest's compositor drops back to the
# normal class, which an unprivileged process may always do.
unrealtime() {
    local pid t
    pid=$(pgrep -P "$(cat "$STATE/pid" 2>/dev/null)" 2>/dev/null | head -1) || return 0
    [ -n "$pid" ] || return 0
    for t in /proc/"$pid"/task/*; do
        chrt -p "${t##*/}" 2>/dev/null | grep -q SCHED_OTHER || chrt -o -p 0 "${t##*/}" 2>/dev/null || true
    done
}

# plasmashell inside the nest, shaped through its scripting interface: a
# plain wallpaper colour, a folder view on an empty directory (not the
# user's ~/Desktop), and one bottom panel holding only a system tray. The
# tray keeps the notifications plasmoid, which shows the app's popups, and
# drops the rest of its stock ones; showAllItems leaves no expander arrow.
shell() {
    # kded6 hosts org.kde.StatusNotifierWatcher, which no service file
    # activates; without it the app finds no system tray.
    run kded6 >/dev/null
    for _ in $(seq 1 40); do
        nq org.kde.StatusNotifierWatcher >/dev/null 2>&1 && break
        sleep 0.25
    done
    run plasmashell --no-respawn >/dev/null
    local ok=0
    for _ in $(seq 1 80); do
        nq org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript "1" >/dev/null 2>&1 && { ok=1; break; }
        sleep 0.25
    done
    [ "$ok" = 1 ] || { echo "plasmashell did not come up; see: dev/nest.sh log"; return 1; }
    nq org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript "
        for (const p of panels()) p.remove();
        for (const d of desktops()) {
            d.currentConfigGroup = ['General'];
            d.writeConfig('url', 'file://$STATE/desktop');
            d.wallpaperPlugin = 'org.kde.color';
            d.currentConfigGroup = ['Wallpaper', 'org.kde.color', 'General'];
            d.writeConfig('Color', '${NEST_WALLPAPER:-#3d5a73}');
            d.reloadConfig();
        }
        const panel = new Panel;
        panel.location = 'bottom';
        panel.height = 44;
        panel.floating = false;
        panel.addWidget('org.kde.plasma.panelspacer');
        const tray = panel.addWidget('org.kde.plasma.systemtray');
        tray.currentConfigGroup = ['General'];
        tray.writeConfig('extraItems', ['org.kde.plasma.notifications']);
        tray.writeConfig('knownItems', ['org.kde.plasma.notifications']);
        tray.writeConfig('showAllItems', true);
        tray.reloadConfig();
    " >/dev/null
    echo "plasmashell up"
}

# Start the tray app against the simulated headset and wait for its control
# interface. Extra arguments go to the app (--verbose).
app() {
    alive || { echo "not up"; return 1; }
    [ -x "$APP" ] || { echo "no $APP: make build" >&2; return 1; }
    BOSECTL_QT_SIM=${BOSECTL_QT_SIM:-qc_ultra2} QT_QPA_PLATFORM=wayland run "$APP" "$@" >/dev/null
    for _ in $(seq 1 50); do
        nq org.bosectl.qt /Sim org.bosectl.qt.Sim.Device >/dev/null 2>&1 && break
        sleep 0.2
    done
    # Connected: the menu shows a battery reading.
    for _ in $(seq 1 50); do
        sim Menu 2>/dev/null | grep -q '^Battery: [0-9]' && { echo "app up"; return 0; }
        sleep 0.2
    done
    echo "app did not connect to the simulated headset; see: dev/nest.sh log"; return 1
}

# org.bosectl.qt /Sim METHOD ARGS...
sim() { local m=$1; shift; nq org.bosectl.qt /Sim "org.bosectl.qt.Sim.$m" "$@"; }

# Every process attached to the nest's private bus: the compositor, the
# clients, and the daemons D-Bus activated there, which outlive the bus
# otherwise. The environment is read whole before it is matched: under
# pipefail a grep -q that stops at the match fails the pipeline once tr is
# still writing.
pids_on_bus() {   # pids_on_bus BUS_ADDRESS
    local addr=$1 p env
    for p in /proc/[0-9]*; do
        env=$({ tr '\0' '\n' < "$p/environ"; } 2>/dev/null) || continue
        if grep -qxF "DBUS_SESSION_BUS_ADDRESS=$addr" <<<"$env"; then
            echo "${p#/proc/}"
        fi
    done
}

down() {
    local addr; addr=$(bus)
    if [ -f "$STATE/pid" ]; then
        pkill -P "$(cat "$STATE/pid")" 2>/dev/null || true
        kill "$(cat "$STATE/pid")" 2>/dev/null || true
    fi
    if [ -n "$addr" ]; then
        local pids; pids=$(pids_on_bus "$addr")
        [ -n "$pids" ] && { kill $pids 2>/dev/null || true; }
        sleep 0.3
        pids=$(pids_on_bus "$addr")
        [ -n "$pids" ] && { kill -9 $pids 2>/dev/null || true; }
    fi
    rm -f "$STATE/pid" "$STATE/bus" "$STATE/clients"
    echo "down"
}

# Kill leftovers from nests whose state is gone. A process belongs to a nest
# only by the nest's own marker, XDG_CONFIG_HOME under
# $XDG_RUNTIME_DIR/bosectl-qt-nest-NAME/, which everything a nest starts
# carries. Running nests are kept.
clean() {
    local rt=${XDG_RUNTIME_DIR:-/tmp} p conf name n=0
    for p in /proc/[0-9]*; do
        conf=$({ cat "$p/environ" 2>/dev/null || true; } | tr '\0' '\n' | sed -n 's/^XDG_CONFIG_HOME=//p')
        case "$conf" in "$rt"/bosectl-qt-nest-*/config) ;; *) continue ;; esac
        name=${conf#"$rt"/bosectl-qt-nest-}; name=${name%/config}
        if [ -f "$rt/bosectl-qt-nest-$name/pid" ] && kill -0 "$(cat "$rt/bosectl-qt-nest-$name/pid")" 2>/dev/null; then
            continue
        fi
        if kill "${p#/proc/}" 2>/dev/null; then n=$((n + 1)); fi
    done
    echo "cleaned $n orphaned processes"
}

run() {
    alive || { echo "not up"; return 1; }
    local b; b=$(nestbus) || return 1
    ( export WAYLAND_DISPLAY=$SOCKET DBUS_SESSION_BUS_ADDRESS=$b $(nest_env) QT_LOGGING_TO_CONSOLE=1; unset DISPLAY
      "${LOW[@]}" "$@" >> "$LOG" 2>&1 & echo $! >> "$STATE/clients" )
    echo "launched: $*"
}

shot() {
    alive || { echo "not up"; return 1; }
    local out=${1:-$STATE/shot-$(date +%H%M%S).png}
    local b; b=$(nestbus) || return 1
    rm -f "$out"
    env -u DISPLAY DBUS_SESSION_BUS_ADDRESS="$b" WAYLAND_DISPLAY=$SOCKET QT_QPA_PLATFORM=wayland $(nest_env) \
        spectacle -b -n -f -o "$out" >/dev/null 2>&1 || true
    [ -s "$out" ] && echo "$out" || { echo "screenshot failed" >&2; return 1; }
}

# A KWin script run through org.kde.kwin.Scripting; its out() lines land in
# the nest log tagged with a fresh marker, which is echoed back.
js() {
    alive || { echo "not up"; return 1; }
    local tag="JS$RANDOM$RANDOM" f="$STATE/script-$RANDOM.js"
    printf 'function out(s) { print("%s " + s); }\n%s\n' "$tag" "$1" > "$f"
    local id; id=$(nq org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$f" "$tag")
    nq org.kde.KWin "/Scripting/Script$id" org.kde.kwin.Script.run >/dev/null
    sleep 0.3
    nq org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript "$tag" >/dev/null
    rm -f "$f"
    grep -a "$tag " "$LOG" | sed "s/.*$tag //"
}

# Geometry of the window whose caption contains TITLE, "X Y W H": the
# client area (wingeom) or the frame with decorations (framegeom).
wingeom() {
    js "for (const w of workspace.windowList()) if (w.caption.indexOf('$1') >= 0 && !w.deleted) { const g = w.clientGeometry; out(Math.round(g.x) + ' ' + Math.round(g.y) + ' ' + Math.round(g.width) + ' ' + Math.round(g.height)); break; }"
}
framegeom() {
    js "for (const w of workspace.windowList()) if (w.caption.indexOf('$1') >= 0 && !w.deleted) { const g = w.frameGeometry; out(Math.round(g.x) + ' ' + Math.round(g.y) + ' ' + Math.round(g.width) + ' ' + Math.round(g.height)); break; }"
}
# Open notification popups, "X Y W H" per line.
notifications() {
    js "for (const w of workspace.windowList()) if ((w.notification || w.criticalNotification) && !w.deleted) { const g = w.frameGeometry; out(Math.round(g.x) + ' ' + Math.round(g.y) + ' ' + Math.round(g.width) + ' ' + Math.round(g.height)); }"
}
# Open menus and submenus, one "X Y W H" per line, oldest first. Tooltips
# are popups too and are left out.
popups() {
    js "for (const w of workspace.windowList()) if (w.popupWindow && !w.tooltip && !w.deleted) { const g = w.frameGeometry; out(Math.round(g.x) + ' ' + Math.round(g.y) + ' ' + Math.round(g.width) + ' ' + Math.round(g.height)); }"
}

# The app's icon, "X Y". The panel is the only dock, a spacer pushes the
# tray to its right end, and the tray shows the notifications bell then the
# app's icon, so the icon is the panel's last square.
tray() {
    local g=""
    for _ in $(seq 1 40); do
        g=$(js "for (const w of workspace.windowList()) if (w.dock) { const g = w.frameGeometry; out(Math.round(g.x + g.width - g.height / 2) + ' ' + Math.round(g.y + g.height / 2)); break; }")
        [ -n "$g" ] && break
        sleep 0.25
    done
    [ -n "$g" ] || { echo "no panel in the nest" >&2; return 1; }
    echo "$g"
}

# A press held long enough for Plasma's items to take it: fakeinput's own
# click (30 ms) moves nothing in plasmashell. click X Y [left|right]
click() {
    printf 'move %s %s\nsleep 120\ndown %s\nsleep 120\nup %s\nsleep 80\n' "$1" "$2" "${3:-left}" "${3:-left}" | input
}

# Inject input through KWin's fake-input protocol. Commands on stdin, or as
# arguments for a single command: move X Y | click [right] | key NAME |
# drag X1 Y1 X2 Y2 | wheel N | sleep MS. Built by `make tools`.
input() {
    [ -x "$FAKEINPUT" ] || { echo "fakeinput not built: make tools" >&2; return 1; }
    if [ $# -gt 0 ]; then
        WAYLAND_DISPLAY=$SOCKET "$FAKEINPUT" "$@"
    else
        WAYLAND_DISPLAY=$SOCKET "$FAKEINPUT"
    fi
}

log() { tail -n "${1:-60}" "$LOG"; }

# Only dispatch when executed, so the file can be sourced for its functions.
[[ "${BASH_SOURCE[0]}" != "$0" ]] && return 0 2>/dev/null

case "${1:-}" in
    up) up ;;
    app) shift; app "$@" ;;
    sim) shift; sim "$@" ;;
    down) down ;;
    run) shift; run "$@" ;;
    shot) shot "${2:-}" ;;
    input) shift; input "$@" ;;
    js) js "$2" ;;
    tray) tray ;;
    click) shift; click "$@" ;;
    popups) popups ;;
    log) log "${2:-60}" ;;
    clean) clean ;;
    bus) bus ;;
    *) sed -n '2,29p' "$0"; exit 1 ;;
esac
