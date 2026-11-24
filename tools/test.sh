#!/usr/bin/env bash
# edwide_screen's test without the game: test/test_app.cc under wine (wine's own d3d11) with the plugin beside it.
# Leaves the pictures grid.bmp (as the game gives it), grid_30.bmp (corrected at 30 degrees) and calibrate_20.bmp,
# calibrate_30.bmp, calibrate_40.bmp (the calibration's shapes at three angles) in the run directory, to look at; a
# second run in no_hook/ with wings_hook = 0. Last tools/doc.sh: the ini made and read with no device, compared with
# test/data.
# Usage: tools/test.sh <build dir> <scratch dir>; environment: EDWIDE_WINEPREFIX (a wine prefix kept between runs),
# WINE_COMMON_TIMEOUT (seconds for one wine run, 600 unless set); with no DISPLAY, WINE_USE_EGL=1 unless set.
# Needs edplugin_common cloned in the checkout (its tools/wine-common.sh).
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
COMMON=$HERE/edplugin_common/tools/wine-common.sh
[[ -f $COMMON ]] || { echo "$0: no $COMMON; in $HERE:" \
  "git clone https://github.com/arturbac/edplugin_common.git edplugin_common" >&2; exit 1; }
. "$COMMON"
BUILD=$(cd "${1:?build dir}" && pwd)
SCR=${2:?scratch dir}
for f in edwide_screen.dll edwide_test.exe edwide_doc.exe; do
  [[ -f $BUILD/$f ]] || { echo "$0: no $BUILD/$f (build first)" >&2; exit 1; }
done
wine_common_prefix EDWIDE_WINEPREFIX
RUN=$(wine_common_run_dir "$SCR" edwide-test)
cp "$BUILD/edwide_screen.dll" "$BUILD/edwide_test.exe" "$RUN/"
set +e
wine_common_run "$RUN" "$RUN/out.txt" -- edwide_test.exe
rc=$?
# a second run with wings_hook = 0: nothing hooked, the picture as it was
mkdir "$RUN/no_hook"
cp "$BUILD/edwide_screen.dll" "$BUILD/edwide_test.exe" "$RUN/no_hook/"
wine_common_run "$RUN/no_hook" "$RUN/out.txt" EDWIDE_TEST_NOHOOK=1 -- edwide_test.exe || rc=1
echo "--- tools/doc.sh: the ini with no device and no drawing" >> "$RUN/out.txt"
"$HERE/tools/doc.sh" "$BUILD" "$SCR" >> "$RUN/out.txt" 2>&1 || rc=1
set -e
cat "$RUN/out.txt"
echo "--- the plugin's log and the pictures in $RUN"
echo "=== overall: $([[ $rc -eq 0 ]] && echo PASSED || echo FAILED)"
exit $rc
