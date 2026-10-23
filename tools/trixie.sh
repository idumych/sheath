#!/bin/sh
# Runs a command inside a local Debian trixie root (= CI's container = VitruvianOS 0.6's
# userland) with Debian's libsword-dev, so tests run in seconds instead of a CI round trip.
#   sh tools/trixie.sh sh tools/test-core.sh [name...]
# First use downloads Debian's official minimal rootfs (debuerreotype, ~50 MB) into
# .deps/trixie and installs the build packages; no root, no host packages (bwrap +
# user namespaces). The project is mounted at the same path; nothing else of $HOME is.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
R=$ROOT/.deps/trixie
PKGS="ca-certificates curl unzip g++ pkg-config python3 sqlite3 libsword-dev diatheke libsqlite3-dev gdb"
URL=https://github.com/debuerreotype/docker-debian-artifacts/raw/dist-amd64/trixie/oci/blobs/rootfs.tar.gz

asroot() {	# run as fake root in the rootfs with network (setup only)
	bwrap --bind "$R" / --dev /dev --proc /proc --tmpfs /tmp --ro-bind /etc/resolv.conf /etc/resolv.conf \
		--unshare-user --uid 0 --gid 0 --setenv PATH /usr/sbin:/usr/bin:/sbin:/bin \
		--setenv DEBIAN_FRONTEND noninteractive "$@"
}

if [ ! -f "$R/.ready" ]; then
	echo "trixie.sh: setting up $R (one time)..." >&2
	rm -rf "$R" && mkdir -p "$R"
	curl -fsSL "$URL" | tar -xz -C "$R" --no-same-owner --exclude='dev/*' 2>/dev/null || true
	[ -x "$R/usr/bin/apt-get" ] || { echo "trixie.sh: rootfs download/extract failed" >&2; exit 1; }
	printf 'APT::Sandbox::User "root";\n' > "$R/etc/apt/apt.conf.d/00sandbox"
	asroot apt-get -q update > "$ROOT/.deps/trixie-setup.log" 2>&1
	asroot apt-get -q install -y --no-install-recommends $PKGS >> "$ROOT/.deps/trixie-setup.log" 2>&1 ||
		{ tail -20 "$ROOT/.deps/trixie-setup.log" >&2; exit 1; }
	touch "$R/.ready"
fi

mkdir -p "$R$ROOT"
exec bwrap --ro-bind "$R" / --dev /dev --proc /proc --tmpfs /tmp --tmpfs /root \
	--ro-bind /etc/resolv.conf /etc/resolv.conf --bind "$ROOT" "$ROOT" --chdir "$PWD" \
	--setenv HOME /root --setenv PATH /usr/bin:/bin --die-with-parent "$@"
