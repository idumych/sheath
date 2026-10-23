#!/bin/sh
# One-command check of a CI-built Sheath on the Renku OS test VM (renku-test, SSH alias
# renku-test, see the haiku-vms skill).
#
#   sh tools/renku-test.sh install [RUN_ID|FILE.hpkg]   install the .hpkg from a haiku.yml run
#                                     (default: newest successful run of this branch), launch,
#                                     screenshot, print OCR text and the app log
#   sh tools/renku-test.sh fresh ...  same as install, after deleting Sheath's settings,
#                                     bookmarks and the user's ~/.sword
#   sh tools/renku-test.sh do STEP... drive the VM (vm.sh steps: m:click,X,Y  t:text  k:1c,9c
#                                     w:1  shot:NAME), then OCR the last shot
#   sh tools/renku-test.sh log        the running app's log
#
# Screenshots and OCR text go to .deps/renku/. Read the PNG to see it; OCR is a rough check.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/.deps/renku
VM=renku-test
P=$HOME/.claude/skills/beos-app-platforms/scripts
R=$HOME/.claude/skills/haiku-release/scripts
mkdir -p "$OUT"

ocr() {
	"$R/ocr-find.sh" "$1" --text > "${1%.png}.txt" 2>/dev/null || true
	echo "--- OCR $(basename "$1")"
	grep -v '^\s*$' "${1%.png}.txt" | head -40
}

shot() {
	VM=$VM SCRATCH=$OUT sh "$P/vm.sh" "shot:$1" > /dev/null
	ocr "$OUT/$1.png"
	echo "--- screenshot: $OUT/$1.png"
}

applog() {
	echo "--- app log"
	ssh $VM 'cat /tmp/sheath.log 2>/dev/null | tail -20'
}

cmd=${1:-install}
[ $# -gt 0 ] && shift
case $cmd in
install|fresh)
	ssh -o ConnectTimeout=5 -o BatchMode=yes $VM true 2>/dev/null || {
		echo "renku-test: VM not reachable (VBoxManage startvm $VM --type headless)"; exit 1; }
	arg=${1:-}
	if [ -n "$arg" ] && [ -f "$arg" ]; then
		pkg=$arg
	else
		run=$arg
		if [ -z "$run" ]; then
			branch=$(git -C "$ROOT" rev-parse --abbrev-ref HEAD)
			run=$(gh run list -R idumych/sheath -w haiku -b "$branch" --status success -L 1 \
				--json databaseId --jq '.[0].databaseId')
		fi
		[ -n "$run" ] || { echo "renku-test: no successful haiku run for this branch"; exit 1; }
		rm -rf "$OUT/pkg" && mkdir -p "$OUT/pkg"
		gh run download "$run" -R idumych/sheath -n sheath-hpkg -D "$OUT/pkg"
		pkg=$(ls "$OUT"/pkg/*.hpkg | head -1)
		echo "run $run: $(gh run view "$run" -R idumych/sheath --json headSha --jq .headSha | cut -c1-8)"
	fi
	echo "package: $(basename "$pkg") $(sha256sum "$pkg" | cut -c1-16)"
	# Haiku's ps: "path [args] team threads gid uid"; kill takes the team id.
	ssh $VM 'hey Sheath quit > /dev/null 2>&1; sleep 1
		for t in $(ps | awk "/apps\/Sheath/ {print \$(NF-3)}"); do kill -9 $t; done; true'
	if [ "$cmd" = fresh ]; then
		ssh $VM 'rm -rf ~/config/settings/Sheath_settings ~/config/settings/Sheath_bookmarks \
			~/config/settings/Sheath_network_ok ~/.sword'
		echo "fresh: settings, bookmarks and ~/.sword removed"
	fi
	scp -q "$pkg" $VM:/boot/home/sheath.hpkg
	# pkgman keeps an installed package with the same name and version, so remove it first.
	ssh $VM 'pkgman uninstall -y sheath > /dev/null 2>&1
		pkgman install -y /boot/home/sheath.hpkg 2>&1 | tail -1
		ls -l /boot/system/packages/sheath-*.hpkg | sed "s/.* \([0-9][0-9]*\) [A-Z][a-z]* .*/installed package: \1 bytes/"
		listattr /boot/system/apps/Sheath | grep -c BEOS: | sed "s/^/attributes: /"
		(Sheath > /tmp/sheath.log 2>&1 &)'
	sleep 4
	shot launch
	applog
	;;
do)
	[ $# -gt 0 ] || { echo "usage: renku-test.sh do STEP..."; exit 1; }
	VM=$VM SCRATCH=$OUT sh "$P/vm.sh" "$@" > /dev/null
	last=""
	for s in "$@"; do case $s in shot:*) last=${s#shot:} ;; esac; done
	if [ -n "$last" ]; then ocr "$OUT/$last.png"; echo "--- screenshot: $OUT/$last.png"; fi
	;;
log)
	applog
	;;
*)
	sed -n 2,14p "$0"; exit 1
	;;
esac
