scenario_desc="a headset that drops off shows as disconnected, and Connect brings it back"
run_scenario() {
    sim SetReachable false >/dev/null
    sim Refresh >/dev/null
    assert_until "menu header says disconnected" menu_has "Bose Headphones (disconnected)"
    assert_true "Power Off disabled" -n "$(sim Menu | grep -x 'Power Off \[disabled\]')"
    menu_open || { fail "menu did not open"; return; }
    snap tray-menu-disconnected $(menus_box)
    sim SetReachable true >/dev/null
    menu_click "Connect"
    assert_until "Connect reconnects" menu_has "Reconnect"
}
