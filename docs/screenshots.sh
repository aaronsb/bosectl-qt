#!/usr/bin/env bash
# Regenerate the README screenshots in docs/media from a dedicated nest.
#
#   docs/screenshots.sh
#
# Runs the UI scenarios (tests/ui) in a nest named "screenshots", then copies
# the stills the README uses. The scenarios take each still before they
# change anything, so every image shows the simulated headset's starting
# state. NEST_SCHEME picks the colour scheme (default BreezeDark, the
# README's look; the goldens use BreezeLight).
set -uo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
export NEST_NAME=screenshots
export NEST_SCHEME=${NEST_SCHEME:-BreezeDark}
export TEST_OUT=$HERE/build/screenshots
export GOLDEN_TOLERANCE=1   # stills, not checks: a different scheme never fails
MEDIA=$HERE/docs/media

"$HERE/tests/ui/run.sh" >/dev/null || echo "note: a scenario failed; check the stills (tests/ui/run.sh shows which)" >&2

mkdir -p "$MEDIA"
for name in tray-menu notification noise-cancellation equalizer mode-manager help; do
    if [ -s "$TEST_OUT/$name.png" ]; then
        cp "$TEST_OUT/$name.png" "$MEDIA/$name.png"
        echo "  docs/media/$name.png"
    else
        echo "  missing: $name" >&2
    fi
done
