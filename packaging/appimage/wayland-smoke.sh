#!/bin/sh
# wayland-smoke.sh — starts the AppImage on a headless Weston (Cosmoe's Wayland backend), checks
# it stays up, and saves a screenshot, the app log and a Wayland protocol trace.
#
#   sh packaging/appimage/wayland-smoke.sh <App>-<version>-x86_64.AppImage [OUT_DIR]
#
# Needs weston (Ubuntu: apt-get install --no-install-recommends weston). Generic haiku-*
# template: appimage.yml runs it after building the AppImage.
set -u
APP=$(readlink -f "$1")
OUT=$(mkdir -p "${2:-wayland-smoke}" && cd "${2:-wayland-smoke}" && pwd)
export XDG_RUNTIME_DIR=$(mktemp -d)
chmod 700 "$XDG_RUNTIME_DIR"
SOCK=wl-smoke

echo "weston tools: $(ls /usr/bin/weston* /usr/libexec/weston* 2>/dev/null | tr '\n' ' ')"
weston --backend=headless --renderer=pixman --width=1280 --height=800 --socket=$SOCK \
	--debug --idle-time=0 > "$OUT/weston.log" 2>&1 &
WESTON=$!
for i in $(seq 40); do [ -S "$XDG_RUNTIME_DIR/$SOCK" ] && break; sleep 0.25; done
if [ ! -S "$XDG_RUNTIME_DIR/$SOCK" ]; then
	echo "wayland-smoke: weston did not start"; cat "$OUT/weston.log"; exit 1
fi

HOME_DIR=$(mktemp -d)
env -u DISPLAY WAYLAND_DISPLAY=$SOCK COSMOE_BACKEND=wayland APPIMAGE_EXTRACT_AND_RUN=1 \
	WAYLAND_DEBUG=client HOME="$HOME_DIR" "$APP" > "$OUT/app.log" 2> "$OUT/wayland-debug.log" &
PID=$!
sleep 15
if ! kill -0 $PID 2>/dev/null; then
	wait $PID; status=$?
	echo "wayland-smoke: the app exited with status $status"
	cat "$OUT/app.log"; tail -30 "$OUT/wayland-debug.log"
	kill $WESTON 2>/dev/null; exit 1
fi
echo "wayland-smoke: the app is running under Wayland"
grep -m3 -E 'xdg_toplevel@[0-9]+\.set_title|xdg_surface@[0-9]+\.ack_configure' "$OUT/wayland-debug.log"

SHOOTER=$(command -v weston-screenshooter || ls /usr/libexec/weston-screenshooter 2>/dev/null | head -1)
if [ -n "$SHOOTER" ]; then
	(cd "$OUT" && WAYLAND_DISPLAY=$SOCK timeout 30 "$SHOOTER") || echo "wayland-smoke: screenshot failed"
else
	echo "wayland-smoke: no weston-screenshooter; no screenshot"
fi
kill $PID 2>/dev/null; sleep 1; kill $WESTON 2>/dev/null
ls -l "$OUT"
if grep -qiE 'error|assert|crash|segment|abort|exception' "$OUT/app.log"; then
	echo "wayland-smoke: problems in the app log:"; grep -iE 'error|assert|crash|segment|abort|exception' "$OUT/app.log"
	exit 1
fi
echo "wayland-smoke: PASS"
