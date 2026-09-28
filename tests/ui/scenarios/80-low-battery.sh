scenario_desc="a battery at or below 15% is flagged in the menu"
run_scenario() {
    sim SetBattery 12 >/dev/null
    sim Refresh >/dev/null
    assert_until "menu flags 12%" menu_has "Battery: 12% ⚠"
}
