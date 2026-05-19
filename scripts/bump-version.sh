#!/usr/bin/env bash
# Bump the version in the VERSION file, commit it, and create a matching
# git tag. Push with `git push origin main --tags` to trigger the release
# workflow.
#
#   scripts/bump-version.sh patch   # 0.1.0 -> 0.1.1  (default)
#   scripts/bump-version.sh minor   # 0.1.0 -> 0.2.0
#   scripts/bump-version.sh major   # 0.1.0 -> 1.0.0

set -euo pipefail
cd "$(dirname "$0")/.."

PART=${1:-patch}
CURRENT=$(tr -d '[:space:]' < VERSION)
IFS=. read -r MAJ MIN PAT <<< "$CURRENT"

case "$PART" in
    major) MAJ=$((MAJ + 1)); MIN=0; PAT=0 ;;
    minor) MIN=$((MIN + 1)); PAT=0 ;;
    patch) PAT=$((PAT + 1)) ;;
    *) echo "usage: $0 [major|minor|patch]"; exit 1 ;;
esac

NEW="$MAJ.$MIN.$PAT"
printf '%s\n' "$NEW" > VERSION

git add VERSION
git commit -m "Release v$NEW"
git tag "v$NEW"

echo
echo "Tagged v$NEW. Push with:"
echo "    git push origin main --tags"
