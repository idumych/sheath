#!/bin/sh
# Builds libsword (if needed) and compiles + runs the Sheath core tests.
# Usage: sh tools/test-core.sh
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

# ---- build libsword if missing ----
if [ ! -f .deps/sword/lib/libsword.so ]; then
  echo "Building libsword…"
  sh tools/build-sword.sh
fi

# ---- prepare fixture modules ----
FIXTURE_DIR="$ROOT/.deps/fixtures/sword"
NEED_FIXTURES=0
[ ! -d "$FIXTURE_DIR" ] && NEED_FIXTURES=1
# Check if KJV module data actually exists (not just conf files)
if [ -d "$FIXTURE_DIR/modules/texts/ztext/kjv" ]; then
  : # OK, data exists
else
  NEED_FIXTURES=1
fi
if [ "$NEED_FIXTURES" -eq 1 ]; then
  echo "Downloading fixture modules…"
  mkdir -p "$FIXTURE_DIR" "$ROOT/.deps/dl"
  cd "$ROOT/.deps/dl"
  [ -f kjv.zip ] || curl -fsSLO -L \
    https://crosswire.org/ftpmirror/pub/sword/packages/rawzip/KJV.zip
  [ -f strongs.zip ] || curl -fsSLO -L \
    https://crosswire.org/ftpmirror/pub/sword/packages/rawzip/StrongsGreek.zip
  rm -rf mods.d modules
  unzip -o kjv.zip
  unzip -o strongs.zip
  cd "$ROOT"
  mkdir -p "$FIXTURE_DIR/mods.d" "$FIXTURE_DIR/modules"
  cp -r "$ROOT/.deps/dl/mods.d"/* "$FIXTURE_DIR/mods.d/" 2>/dev/null || true
  cp -r "$ROOT/.deps/dl/modules"/* "$FIXTURE_DIR/modules/" 2>/dev/null || true
  echo "Fixtures ready: $FIXTURE_DIR"
fi

# ---- compile test ----
echo "Compiling test_core…"
CXX="${CXX:-g++}"
SWORD_INC="-I.deps/sword/include -I.deps/sword/include/sword"
CORE_INC="-Isrc/core"
OUT="$ROOT/.deps/test_core"
$CXX -std=gnu++17 $SWORD_INC $CORE_INC -o "$OUT" \
  tests/test_core.cpp \
  src/core/Sword.cpp src/core/Markup.cpp src/core/Ref.cpp \
  -L.deps/sword/lib -lsword -lbz2 -lz

# ---- run tests ----
echo "Running tests…"
export SWORD_PATH="$FIXTURE_DIR"
export LD_LIBRARY_PATH="$ROOT/.deps/sword/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Run with timeout, capture output
TMPLOG="$ROOT/.deps/tmp/test_output.log"
mkdir -p "$ROOT/.deps/tmp"
EXITCODE=0
if ! timeout 120 "$OUT" > "$TMPLOG" 2>&1; then
  EXITCODE=$?
  if [ "$EXITCODE" -eq 124 ]; then
    echo "Test binary timed out after 120s"
  else
    echo "Test binary crashed or returned non-zero"
  fi
fi

# Count PASS/FAIL from output lines matching the test format
TOTAL=0; PASS=0; FAIL=0
if grep -q '^  PASS:' "$TMPLOG" 2>/dev/null; then
  TOTAL=$(grep -c '^  PASS:' "$TMPLOG")
  PASS=$TOTAL
fi
if grep -q '^  FAIL:' "$TMPLOG" 2>/dev/null; then
  FAIL=$(grep -c '^  FAIL:' "$TMPLOG")
fi

# If the binary crashed or timed out, count as a failure
if [ "$EXITCODE" -ne 0 ]; then
  FAIL=$((FAIL + 1))
  TOTAL=$((PASS + FAIL))
fi

# If no tests ran at all (TOTAL == 0), that's a failure
if [ "$TOTAL" -eq 0 ]; then
  FAIL=1
  TOTAL=0
fi

echo ""
cat "$TMPLOG"
echo ""
echo "TOTAL $TOTAL PASS $PASS FAIL $FAIL"

# Exit 1 if failures or no tests
if [ "$FAIL" -gt 0 ] || [ "$TOTAL" -eq 0 ]; then
  exit 1
fi

exit 0
