#!/bin/sh
# Fails if any file outside a task's allowed set was changed, added or deleted (per git).
#   sh tools/check-scope.sh 'src/core/Layout\.(h|cpp)$' 'tests/samples/'
# Arguments are grep -E patterns matched against repo-relative paths.
cd "$(dirname "$0")/.." || exit 1
bad=0
for path in $(git status --porcelain --untracked-files=all | cut -c4- | sed 's/.* -> //'); do
	ok=0
	for pat in "$@"; do echo "$path" | grep -qE "$pat" && { ok=1; break; }; done
	[ $ok -eq 1 ] || { echo "SCOPE VIOLATION: $path"; bad=1; }
done
[ $bad -eq 0 ] && echo "SCOPE OK"
exit $bad
