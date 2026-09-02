#!/usr/bin/env bash
# Usage: ./scripts/release.sh 1.2.7
# Bumps [crossink] version in platformio.ini, commits, and creates the git tag.
set -euo pipefail

VERSION="${1:-}"
if [[ -z "$VERSION" ]]; then
  echo "Usage: $0 <version>  (e.g. 1.2.7)" >&2
  exit 1
fi

sed -i '' "/^\\[crossink\\]/,/^\\[/ s/^version = .*/version = $VERSION/" platformio.ini
git add platformio.ini
git commit -m "Update crossink version to $VERSION"
git tag "v$VERSION"
echo "Tagged v$VERSION — push with: git push && git push origin v$VERSION"
