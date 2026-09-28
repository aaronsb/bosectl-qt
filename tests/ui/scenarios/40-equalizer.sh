scenario_desc="Equalizer: bands show the headset's EQ; Try writes it; Reset flattens it"
run_scenario() {
    local t="Equalizer - bosectl"
    open_window "Equalizer..." "$t" || { fail "window did not open"; return; }
    assert_until "bass slider at +2" widgets_match "$t" 'QSlider.* value=2$' 
    snap equalizer $(framegeom "$t")
    click_widget "$t" QPushButton Reset
    assert_until "Reset flattens the headset's EQ" dev_is eq 0,0,0
    click_widget "$t" QSlider
    input key up
    input key up
    click_widget "$t" QPushButton Try
    assert_until "Try writes the moved band" dev_is eq 2,0,0
}
