#!/usr/bin/env bash
# The correction's cost on the graphics card at a picture's size, under DXVK as the game is (its d3d11.dll and
# dxgi.dll copied from a game's prefix): a frame's time with the correction less without it, median of 100.
# Usage: tools/bench.sh <build dir> <scratch dir> [<width> <height>] (default 1920 1080)
# Environment: EDWIDE_WINEPREFIX; EDWIDE_DXVK_DIR (a folder with DXVK's d3d11.dll and dxgi.dll)
set -euo pipefail
BUILD=$(cd "${1:?build dir}" && pwd)
SCR=${2:?scratch dir}
WIDTH=${3:-1920}
HEIGHT=${4:-1080}
[[ -n ${EDWIDE_WINEPREFIX:-} ]] || { echo "$0: set EDWIDE_WINEPREFIX" >&2; exit 1; }
[[ -n ${EDWIDE_DXVK_DIR:-} && -f $EDWIDE_DXVK_DIR/d3d11.dll && -f $EDWIDE_DXVK_DIR/dxgi.dll ]] ||
  { echo "$0: set EDWIDE_DXVK_DIR to a folder with DXVK's d3d11.dll and dxgi.dll" >&2; exit 1; }
export WINEPREFIX=$EDWIDE_WINEPREFIX
RUN=$(mktemp -d "$SCR/edwide-bench.XXXXXX")
cp "$BUILD/edwide_screen.dll" "$BUILD/edwide_test.exe" "$EDWIDE_DXVK_DIR/d3d11.dll" "$EDWIDE_DXVK_DIR/dxgi.dll" "$RUN/"
(cd "$RUN" && EDWIDE_BENCH="${WIDTH}x${HEIGHT}" WINEDEBUG=-all DXVK_LOG_LEVEL=none \
   WINEDLLOVERRIDES="d3d11,dxgi=n" wine edwide_test.exe > out.txt 2>&1) || true
grep -E '^bench gpu:|FAIL' "$RUN/out.txt" || { echo "$0: no result (see $RUN/out.txt)" >&2; exit 1; }
