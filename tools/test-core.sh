#!/bin/sh
# Builds and runs Sheath's host-side tests. The only way to run them.
#   sh tools/test-core.sh [name...]     e.g. sh tools/test-core.sh layout
# Runs in CI only (tools/ci-test.sh). Every tests/test_<name>.cpp is linked with all of src/core/*.cpp, libsword and sqlite3,
# and run with a timeout (TEST_TIMEOUT, default 120 s). Test lines look like "  PASS: <name>" / "  FAIL: <name>: ...".
# Prints a last line "TOTAL <n> PASS <p> FAIL <f>"; exits 1 if f > 0 or n == 0.
# A binary that fails to build, crashes or times out counts as a FAIL line.
# Fixtures (.deps/fixtures/sword: CrossWire KJV + StrongsGreek) are read-only; each binary gets
# its own empty scratch dir in $SHEATH_TMP (.deps/tmp/<name>) for anything it writes.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
D=$ROOT/.deps

# Runs where libsword is installed: CI (Debian trixie, Haiku R1/beta6) or tools/trixie.sh.
pkg-config --exists sword || { echo "  FAIL: libsword-dev not installed (run in CI: tools/ci-test.sh)"; echo "TOTAL 1 PASS 0 FAIL 1"; exit 1; }
DIATHEKE=$(command -v diatheke) || { echo "  FAIL: diatheke not installed"; echo "TOTAL 1 PASS 0 FAIL 1"; exit 1; }
export DIATHEKE

FIX=$D/fixtures/sword
if [ ! -f "$FIX/mods.d/kjv.conf" ]; then
	echo "Preparing fixture modules..."
	mkdir -p "$D/dl" "$FIX"
	for m in KJV StrongsGreek; do
		[ -s "$D/dl/$m.zip" ] || curl -fsSL -o "$D/dl/$m.zip" \
			"https://crosswire.org/ftpmirror/pub/sword/packages/rawzip/$m.zip" ||
			{ echo "  FAIL: fixture download $m"; echo "TOTAL 1 PASS 0 FAIL 1"; exit 1; }
		unzip -q -o "$D/dl/$m.zip" -d "$FIX"
	done
	chmod -R a-w "$FIX"
fi

mkdir -p "$D/gen" "$D/tmp" "$D/bin"
if [ -f src/core/bookmarks_schema.sql ]; then
	{ printf 'static const char* kBookmarksSchema = R"SQL('; cat src/core/bookmarks_schema.sql; printf ')SQL";\n'; } \
		> "$D/gen/bookmarks_schema.h"
fi

CXX=${CXX:-g++}
CFLAGS="-std=c++17 -Wall -Wno-deprecated-declarations -g $(pkg-config --cflags sword) -Isrc/core -I$D/gen"
LIBS="$(pkg-config --libs sword) -lsqlite3"
export SWORD_PATH=$FIX

if [ $# -gt 0 ]; then names=$*; else
	names=$(for f in tests/test_*.cpp; do n=${f#tests/test_}; echo "${n%.cpp}"; done)
fi
LOG=$D/tmp/test-core.log
: > "$LOG"
for n in $names; do
	echo "== test_$n" | tee -a "$LOG"
	if ! $CXX $CFLAGS -o "$D/bin/test_$n" "tests/test_$n.cpp" src/core/*.cpp $LIBS > "$D/tmp/build_$n.log" 2>&1; then
		grep -m 15 -E 'error' "$D/tmp/build_$n.log"
		echo "  FAIL: test_$n build failed (see .deps/tmp/build_$n.log)" | tee -a "$LOG"
		continue
	fi
	rm -rf "$D/tmp/$n" && mkdir -p "$D/tmp/$n"
	SHEATH_TMP=$D/tmp/$n timeout "${TEST_TIMEOUT:-120}" "$D/bin/test_$n" > "$D/tmp/out_$n.log" 2>&1
	rc=$?
	tee -a "$LOG" < "$D/tmp/out_$n.log" | grep -E '^  (PASS|FAIL):'
	[ $rc -eq 0 ] || echo "  FAIL: test_$n exited with status $rc$( [ $rc -eq 124 ] && echo ' (timeout)')" | tee -a "$LOG"
done

p=$(grep -c '^  PASS:' "$LOG")
f=$(grep -c '^  FAIL:' "$LOG")
echo "TOTAL $((p + f)) PASS $p FAIL $f"
[ "$f" -eq 0 ] && [ $((p + f)) -gt 0 ]
