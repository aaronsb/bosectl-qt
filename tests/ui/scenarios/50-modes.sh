scenario_desc="Modes: presets and custom profiles listed; selecting one and Activate switches the headset"
run_scenario() {
    local t="Modes - bosectl"
    open_window "Modes..." "$t" || { fail "window did not open"; return; }
    assert_until "profiles listed" widgets_match "$t" 'QListWidget.*\[quiet'
    # Rows: quiet aware immersion cinema Commute Focus. End, then up one.
    click_widget "$t" QListWidget
    input key end
    input key up
    assert_until "Commute selected" widgets_match "$t" 'QListWidget.*\[Commute\]'
    assert_until "a custom profile's editor is live" widgets_match "$t" 'QPushButton.*\[Delete\]$'
    snap mode-manager $(framegeom "$t")
    click_widget "$t" QPushButton Activate
    assert_until "Activate switches the headset to slot 4" dev_is mode 4
    assert_until "the mode's level applies" dev_is cnc 7
}
