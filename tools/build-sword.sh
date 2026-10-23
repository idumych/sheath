#!/bin/sh
# Builds libsword 1.9.0 (the version in Debian trixie, VitruvianOS's userland)
# into .deps/sword for host-side tests. Release builds use Debian's libsword-dev.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
D=$ROOT/.deps
SHA=42409cf3de2faf1108523e2c5ac0745d21f9ed2a5c78ed878ee9dcc303426b8a
[ -f "$D/sword/lib/libsword.so" ] && { echo "libsword already built: $D/sword"; exit 0; }
mkdir -p "$D"
cd "$D"
[ -f sword-1.9.0.tar.gz ] || curl -fsSLO https://crosswire.org/ftpmirror/pub/sword/source/v1.9/sword-1.9.0.tar.gz
echo "$SHA  sword-1.9.0.tar.gz" | sha256sum -c -
rm -rf sword-1.9.0 sword-build
tar xzf sword-1.9.0.tar.gz
cmake -S sword-1.9.0 -B sword-build -G "Unix Makefiles" \
  -DCMAKE_INSTALL_PREFIX="$D/sword" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DLIBSWORD_LIBRARY_TYPE=Shared -DSWORD_NO_CLUCENE=Yes -DSWORD_BUILD_UTILS=Yes \
  -DSWORD_BUILD_TESTS=No -DSWORD_BUILD_EXAMPLES=No \
  -DCMAKE_CXX_FLAGS="-std=gnu++17 -Wno-deprecated-declarations" > sword-cmake.log 2>&1 \
  || { tail -30 sword-cmake.log; exit 1; }
make -C sword-build -j"$(nproc)" > sword-make.log 2>&1 || { tail -40 sword-make.log; exit 1; }
make -C sword-build install > sword-install.log 2>&1 || { tail -20 sword-install.log; exit 1; }
ls "$D/sword/lib"/libsword* && echo "libsword built: $D/sword"
