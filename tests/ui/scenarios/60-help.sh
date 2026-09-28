scenario_desc="Help opens from the menu"
run_scenario() {
    open_window "Help..." "Help" || { fail "Help did not open"; return; }
    ok "Help window maps"
    local title; title=$(sim Windows | grep -m1 Help)
    snap help $(framegeom "$title")
}
