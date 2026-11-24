#!/usr/bin/env bash
# edwide_screen's settings without the game, with no device and no drawing: test/doc_app.cc under wine calls the
# plugin's export edwide_screen_write_settings_doc, which makes the log and the ini as at the game's start. Checked: a
# fresh ini is test/data/fresh.ini; an ini already there is left byte for byte as it was; an ini of wrong lines
# (test/data/bad.ini) gives read_ini's log lines test/data/bad_log.txt.
# Usage: tools/doc.sh <build dir> <scratch dir>; environment: EDWIDE_WINEPREFIX (a wine prefix kept between runs),
# WINE_COMMON_TIMEOUT (seconds for one wine run, 600 unless set). Needs edplugin_common cloned in the checkout (its
# tools/wine-common.sh). tools/test.sh runs it too.
set -euo pipefail
# the shell's environment would move the runs' files out of their folders: edloader's folders for the ini and the log
unset EDLOADER_CONFIG_DIR EDLOADER_LOG_DIR
HERE=$(cd "$(dirname "$0")/.." && pwd)
COMMON=$HERE/edplugin_common/tools/wine-common.sh
[[ -f $COMMON ]] || { echo "$0: no $COMMON; in $HERE:" \
  "git clone https://github.com/arturbac/edplugin_common.git edplugin_common" >&2; exit 1; }
. "$COMMON"
BUILD=$(cd "${1:?build dir}" && pwd)
SCR=${2:?scratch dir}
for f in edwide_screen.dll edwide_doc.exe; do
  [[ -f $BUILD/$f ]] || { echo "$0: no $BUILD/$f (build first)" >&2; exit 1; }
done
wine_common_prefix EDWIDE_WINEPREFIX
RUN=$(wine_common_run_dir "$SCR" edwide-doc)
rc=0
check() {  # <0 = ok> <what>
  if [[ $1 -eq 0 ]]; then echo "ok  : $2"; else echo "FAIL: $2"; rc=1; fi
}
# a case: its own folder with the plugin and the program, the ini copied from test/data beforehand or none
doc_run() {  # <name> [the ini in test/data]
  mkdir "$RUN/$1"
  cp "$BUILD/edwide_screen.dll" "$BUILD/edwide_doc.exe" "$RUN/$1/"
  [[ $# -lt 2 ]] || cp "$HERE/test/data/$2" "$RUN/$1/edwide_screen.ini"
  wine_common_run "$RUN/$1" "$RUN/$1/out.txt" -- edwide_doc.exe
}
# read_ini's log lines: the log's settings lines without their time and without the ini's path
settings_lines() {  # <case>
  grep 'settings: ' "$RUN/$1/edwide_screen.log" | cut -d' ' -f2- | grep -v '\.ini' | tr -d '\r'
}
set +e
doc_run fresh
check $? "fresh: the export ran"
cmp -s "$RUN/fresh/edwide_screen.ini" "$HERE/test/data/fresh.ini"
check $? "fresh: the ini is test/data/fresh.ini"
doc_run kept kept.ini
check $? "kept: the export ran"
cmp -s "$RUN/kept/edwide_screen.ini" "$HERE/test/data/kept.ini"
check $? "kept: the ini there is left as it was"
settings_lines kept | cmp -s - "$HERE/test/data/kept_log.txt"
check $? "kept: read_ini's log lines are test/data/kept_log.txt"
doc_run bad bad.ini
check $? "bad: the export ran"
settings_lines bad | cmp -s - "$HERE/test/data/bad_log.txt"
check $? "bad: read_ini's log lines are test/data/bad_log.txt"
cmp -s "$RUN/bad/edwide_screen.ini" "$HERE/test/data/bad.ini"
check $? "bad: the ini there is left as it was"
set -e
echo "=== doc: $([[ $rc -eq 0 ]] && echo PASSED || echo FAILED) (run directory $RUN)"
exit $rc
