# Testing

Three layers, none of which needs a headset. The first two run in CI; the
third needs a Plasma desktop and runs locally.

| Layer | Command | What it covers |
|---|---|---|
| Library | `make test` (`bmap_tests`) | The vendored bosectl C++ suite, against the submodule commit this app pins |
| App, offscreen | `make test` (the other four) | Worker logic, autostart, help content, and the tray, its menu and its windows against the simulated headset |
| UI, nested | `make test-ui` | The real tray in a nested Plasma: a right-click opens Plasma's menu, clicks reach the headset, and screenshots match the goldens |

## The simulated headset

`BOSECTL_QT_SIM=qc_ultra2` runs the app against `SimDevice`
(`src/SimDevice.h`), a stateful QC Ultra 2. It answers the BMAP packets
`BmapConnection` sends with the replies a real headset gives. A SETGET
changes what the next GET returns, a mode switch applies that mode's
settings, and a profile write fills a slot. `BmapWorker` gets it through its
`Connector` seam; Bluetooth is the default connector. In sim mode the tray
publishes nothing to BlueZ, because the system bus is the real one even in a
nest.

```bash
make sim    # the tray in your live session, backed by the simulated headset
```

A sim-mode app exports `org.bosectl.qt /Sim` on its session bus:

| Method | Does |
|---|---|
| `Menu` | the tray menu, one entry per line, two spaces per level, `[disabled]` and `[checked]` marked |
| `Trigger PATH` | trigger an entry: `Noise Cancellation...`, `Spatial Audio/Room`, `About/About bosectl-qt...` |
| `Windows` | visible window titles |
| `Widgets TITLE` | a window's controls: class, geometry in the client area, text, value, state |
| `Close [TITLE]` | close one window, or all of them |
| `Grab TITLE PATH` | render a window to PNG without decorations |
| `Tooltip`, `Notify` | the tray tooltip; what a left-click does |
| `Device` | the headset's state as `key=value` fields |
| `Reset`, `SetBattery N`, `SetReachable BOOL`, `SetLatency MS`, `Refresh` | steer the headset |

`tests/test_tray.cpp` drives the app through this surface offscreen. It
covers menu state when connected, disconnected and at low battery; toggles,
submenus, rename, power off and start on login; Noise Cancellation,
Equalizer and Modes (activate, edit, create, delete); and the busy
indicator.

## The nest

`dev/nest.sh` is adapted from kwin-canvas's harness. `up` starts a
`kwin_wayland` on its own D-Bus session bus, with config, state, cache and
data homes under `$XDG_RUNTIME_DIR/bosectl-qt-nest-NAME`, then starts
`kded6` and `plasmashell` inside it. plasmashell is shaped through
`evaluateScript`: a plain wallpaper, a folder view on an empty directory,
and one full-width bottom panel holding only a system tray with the
notifications bell. `app` starts `build/bosectl-qt` against the simulated
headset and waits until it has connected.

```bash
make nest                          # windowed on your desktop, NEST_NAME=dev
NEST_NAME=dev dev/nest.sh sim Menu
NEST_NAME=dev dev/nest.sh shot     # spectacle against the nested compositor
make nest-down
```

The nest never reads your settings or shows your data:

- **Fixed look.** `kdeglobals` pins Breeze, Noto Sans 10, the Breeze icons
  and `NEST_SCHEME` (default `BreezeLight`), and sets animations to zero, so
  a screenshot never catches a fade.
- **No portal.** The nest's bus gets its own service directory without the
  portals. The portal's background monitor reads systemd's app scopes and
  would put the host's running flatpaks in the nested tray.
- **Empty desktop.** The folder view points at an empty directory, not
  `~/Desktop`.
- **Separate processes.** Everything runs under `nice -n 10 ionice -c3`.
  `down` kills every process on the nest's bus, and `dev/nest.sh clean`
  sweeps orphans by their `XDG_CONFIG_HOME` marker.

`dev/nest.sh` is sourceable. Its helpers include `click X Y [right]`, a
press held for 120 ms because plasmashell ignores fakeinput's 30 ms click;
`tray`, the icon's centre; `popups`, open menus without tooltips;
`notifications`; `wingeom`/`framegeom TITLE`; and `js 'CODE'`, a KWin script
whose `out()` lines come back.

`tools/fakeinput.c` is copied from kwin-canvas and injects input through
`org_kde_kwin_fake_input`. The nest runs KWin with
`KWIN_WAYLAND_NO_PERMISSION_CHECKS=1`, so the tool needs no `.desktop`
registration. `fakeinput.c` and `keys.h` are MIT. The protocol file,
`tools/protocols/fake-input.xml`, is KDE's and stays LGPL-2.1-or-later
(`LICENSES/`). The tool is a separate test program, and nothing of it links
into the app.

## UI scenarios

```bash
make test-ui                     # all of tests/ui/scenarios
make test-ui ARGS=modes          # file names matching "modes"
make test-ui ARGS=--keep         # leave the nest up (NEST_NAME=test) to poke at
make golden                      # re-record tests/ui/golden
```

`tests/ui/run.sh` starts the `test` nest with `NEST_VIRTUAL=1`, which renders
offscreen at full frame rate, then starts the app. For each
`tests/ui/scenarios/NN-name.sh` it runs `reset_app` (menus and windows
closed, headset reachable and reset) and calls `run_scenario`. Scenarios use
`tests/ui/lib.sh`:

- `menu_open`, `menu_close`, `menu_click LABEL`, `menu_entry LABEL`: the
  tray menu through a real right-click. Plasma draws it from the app's
  dbusmenu, and entry positions come from `sim Menu`: 29 px rows,
  separators add 5.
- `open_window ENTRY CAPTION`, `click_widget CAPTION CLASS [TEXT]`,
  `widget_at`: windows, with widget positions from `sim Widgets` plus KWin's
  client geometry.
- `dev FIELD`, `dev_is FIELD VALUE`, `menu_has LINE`, `widgets_match`.
- `assert_eq`, `assert_true`, `assert_until NAME CMD...` (polls for 5 s).
- `snap NAME [X Y W H]`: park the pointer, screenshot, crop, and compare with
  `tests/ui/golden/NAME.png` by normalised RMSE within `GOLDEN_TOLERANCE`
  (default 0.02). Diffs land in `build/test-ui/NAME.diff.png`.

Two runs on the same machine match at RMSE 0. The goldens depend on this
machine's fonts and Plasma version: re-record with `make golden` after an
intentional visual change or a Plasma upgrade, and look at the images before
committing them.

## Screenshots

`make screenshots` runs the scenarios in a `screenshots` nest with
`NEST_SCHEME=BreezeDark`, then copies the stills into `docs/media`. Each
scenario takes its still before it changes anything, so every image shows
the simulated headset's starting state.

## Things that cost time

- **Plasma opens submenus on click.** A pointer that arrives on a submenu
  entry in one jump highlights it without opening it.
- **Tooltips are popups too.** Hovering the tray icon opens one, and KWin
  reports it as `popupWindow`; `popups` filters on `w.tooltip`.
- **kded6 is not bus-activated for the tray.** `org.kde.StatusNotifierWatcher`
  has no service file. Without `kded6` running, the app finds no system tray
  and blocks on its "System tray not available" dialog.
- **`SNI.ContextMenu` is not a right-click.** Plasma draws the exported
  dbusmenu itself. Calling `ContextMenu` over D-Bus makes Qt pop up its own
  QMenu, which fails on Wayland without a parent.
