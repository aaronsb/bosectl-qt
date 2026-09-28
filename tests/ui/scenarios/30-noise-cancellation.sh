scenario_desc="Noise Cancellation: the slider starts at the headset's level; Apply writes it"
run_scenario() {
    local t="Noise Cancellation - bosectl"
    open_window "Noise Cancellation..." "$t" || { fail "window did not open"; return; }
    assert_until "slider shows the headset's level" widgets_match "$t" 'QSlider.* value=10' 
    snap noise-cancellation $(framegeom "$t")
    # Keyboard on the focused slider, then a real click on Apply.
    click_widget "$t" QSlider
    input key home
    assert_until "slider moved to 0" widgets_match "$t" 'QSlider.* value=0'
    assert_eq "dragging alone writes nothing" "$(dev cnc)" 10
    click_widget "$t" QPushButton Apply
    assert_until "Apply writes the level" dev_is cnc 0
    click_widget "$t" QPushButton Close
    assert_until "Close closes it" test -z "$(wingeom "$t")"
}
