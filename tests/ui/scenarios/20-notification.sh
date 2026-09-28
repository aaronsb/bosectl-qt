scenario_desc="a left-click on the tray icon shows the status notification"
run_scenario() {
    read -r x y < <(tray)
    click "$x" "$y"
    assert_until "notification popup appears" notifications_open
    read -r nx ny nw nh < <(notifications | head -1)
    assert_true "notification fits on screen" $((nx + nw)) -le "$WIDTH"
    snap notification "$nx" "$ny" "$nw" "$nh"
}
