scenario_desc="a right-click on the tray icon opens the menu Plasma draws from the app's dbusmenu; entries write through to the headset"
run_scenario() {
    menu_open || { fail "menu did not open"; return; }
    ok "menu opens on right-click"
    snap tray-menu $(menus_box)

    menu_click "Wind Block"
    assert_until "Wind Block click reaches the headset" dev_is wind 0
    assert_until "menu shows Wind Block cleared" menu_has "Wind Block"
    assert_until "the menu closes after a click" test -z "$(popups)"

    menu_open
    menu_click "Multipoint"
    assert_until "Multipoint click reaches the headset" dev_is multipoint 0
}
