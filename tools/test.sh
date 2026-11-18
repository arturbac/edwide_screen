#!/usr/bin/env bash
# edwide_screen's test without the game: test/test_app.cc under wine (wine's own d3d11) with the plugin beside it.
# Leaves the pictures grid.bmp (as the game gives it), grid_30.bmp (corrected at 30 degrees) and calibrate_20.bmp,
# calibrate_30.bmp, calibrate_40.bmp (the calibration's shapes at three angles) in the run directory, to look at.
# Usage: tools/test.sh <build dir> <scratch dir>; environment: EDWIDE_WINEPREFIX (a wine prefix kept between runs)
set -euo pipefail
BUILD=$(cd "${1:?build dir}" && pwd)
SCR=${2:?scratch dir}
[[ -n ${EDWIDE_WINEPREFIX:-} ]] || { echo "$0: set EDWIDE_WINEPREFIX" >&2; exit 1; }
for f in edwide_screen.dll edwide_test.exe; do
  [[ -f $BUILD/$f ]] || { echo "$0: no $BUILD/$f (build first)" >&2; exit 1; }
done
export WINEPREFIX=$EDWIDE_WINEPREFIX
RUN=$(mktemp -d "$SCR/edwide-test.XXXXXX")
cp "$BUILD/edwide_screen.dll" "$BUILD/edwide_test.exe" "$RUN/"
set +e
(cd "$RUN" && WINEDEBUG=-all wine edwide_test.exe > out.txt 2>&1)
rc=$?
set -e
cat "$RUN/out.txt"
echo "--- the plugin's log and the pictures in $RUN"
echo "=== overall: $([[ $rc -eq 0 ]] && echo PASSED || echo FAILED)"
exit $rc
