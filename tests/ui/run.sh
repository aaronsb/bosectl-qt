#!/usr/bin/env bash
# Run the UI scenarios against the tray app in a dedicated nest.
#
#   tests/ui/run.sh [--update-golden] [--keep] [PATTERN]
#
# --update-golden  store this run's screenshots as the golden references
# --keep           leave the test nest running afterwards (dev/nest.sh with
#                  NEST_NAME=test drives it)
# PATTERN          only scenarios whose file name matches
#
# The app is started once and each scenario begins from reset_app, so the
# simulated headset's settings carry across scenarios in file order.
set -uo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
KEEP=0; PATTERN=""
for a in "$@"; do
    case $a in
        --update-golden) export UPDATE_GOLDEN=1 ;;
        --keep) KEEP=1 ;;
        *) PATTERN=$a ;;
    esac
done
# shellcheck source=lib.sh
source "$HERE/tests/ui/lib.sh"

echo "== bosectl-qt UI tests (nest: $NEST_NAME, out: $OUT)"
down >/dev/null 2>&1 || true
up || exit 1
app || { log 40; down >/dev/null; exit 1; }

for f in "$HERE"/tests/ui/scenarios/*.sh; do
    name=$(basename "$f" .sh)
    [ -n "$PATTERN" ] && [[ "$name" != *$PATTERN* ]] && continue
    unset -f run_scenario; scenario_desc=""
    # shellcheck source=/dev/null
    source "$f"
    echo "-- $name: $scenario_desc"
    reset_app
    run_scenario
done

echo "== pass $PASS  fail $FAIL  skip $SKIP"
[ "$KEEP" = 1 ] || down >/dev/null
[ "$FAIL" = 0 ]
