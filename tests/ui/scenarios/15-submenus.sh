scenario_desc="submenus open on click and their radio entries set spatial audio and sidetone"
# submenu_pick ENTRY ROW: open ENTRY's submenu with a click, then click the
# submenu's ROW (0-based). Plasma opens a submenu on click, not on a hover
# the pointer arrives at in one jump.
submenu_pick() {
    menu_open || { fail "menu did not open"; return 1; }
    menu_click "$1"
    for _ in $(seq 1 25); do [ "$(popups | wc -l)" -ge 2 ] && break; sleep 0.2; done
    [ "$(popups | wc -l)" -ge 2 ] || { fail "$1 submenu did not open"; return 1; }
    [ -n "${3:-}" ] && snap "$3" $(menus_box)
    read -r sx sy sw sh < <(popups | tail -1)
    click $((sx + sw / 2)) $((sy + 18 + 29 * $2))
}

run_scenario() {
    submenu_pick "Spatial Audio" 2 tray-menu-spatial && ok "Spatial Audio submenu opens"
    assert_until "Head Tracking reaches the headset" dev_is spatial 2
    assert_until "menu marks Head Tracking" menu_has "  Head Tracking [checked]"

    # Sidetone: Off, Low, Medium, High. Row 3 is High.
    submenu_pick "Sidetone" 3 && ok "Sidetone submenu opens"
    assert_until "High reaches the headset" dev_is sidetone 1
    assert_until "menu marks High" menu_has "  High [checked]"
}
