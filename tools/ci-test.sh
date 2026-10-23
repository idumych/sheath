#!/bin/sh
# Runs the tests in GitHub CI on the current working tree (committed or not) and prints the
# result. Nothing is built locally.
#   sh tools/ci-test.sh
# Snapshots the working tree (minus .gitignore'd files) as a commit on the branch ci-scratch
# (a normal fast-forward push; main is never touched), waits for the `test` workflow on both
# architectures, and prints every PASS/FAIL/TOTAL line per architecture, plus compiler errors.
# Exit 0 only if both architectures passed.
set -u
cd "$(dirname "$0")/.." || exit 1
IDX=$(mktemp)
trap 'rm -f "$IDX"' EXIT
cp .git/index "$IDX" 2>/dev/null
GIT_INDEX_FILE=$IDX git add -A . >/dev/null 2>&1
tree=$(GIT_INDEX_FILE=$IDX git write-tree) || exit 1
git fetch -q origin ci-scratch 2>/dev/null
parent=$(git rev-parse -q --verify FETCH_HEAD 2>/dev/null || git rev-parse HEAD)
commit=$(git commit-tree "$tree" -p "$parent" -m "CI snapshot $(date -u +%FT%TZ)") || exit 1
git push -q origin "$commit:refs/heads/ci-scratch" || { echo "CI: push failed"; exit 1; }
echo "CI: pushed snapshot ${commit%"${commit#????????}"}, waiting for the test workflow..."

run=""
for _ in $(seq 60); do
	run=$(gh run list --workflow test.yml --branch ci-scratch --json databaseId,headSha \
		--jq ".[] | select(.headSha == \"$commit\") | .databaseId" 2>/dev/null | head -1)
	[ -n "$run" ] && break
	sleep 5
done
[ -n "$run" ] || { echo "CI: no workflow run appeared for $commit"; exit 1; }
for _ in $(seq 120); do
	st=$(gh run view "$run" --json status --jq .status 2>/dev/null)
	[ "$st" = completed ] && break
	sleep 15
done
[ "$st" = completed ] || { echo "CI: run $run did not finish in 30 min"; exit 1; }

concl=$(gh run view "$run" --json conclusion --jq .conclusion)
gh run view "$run" --log 2>/dev/null | sed 's/\x1b\[[0-9;]*m//g' |
	awk -F'\t' '{ job=$1; sub(/.*\(/, "", job); sub(/[,)].*/, "", job); line=$3; sub(/^[0-9TZ:.-]+ /, "", line);
		if (line ~ /^  (PASS|FAIL):|^TOTAL |^== test_| error: |^E: |Unable to locate/) print job ": " line }'
echo "CI: run $run conclusion: $concl  (gh run view $run --log for everything)"
[ "$concl" = success ]
