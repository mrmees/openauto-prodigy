#!/usr/bin/env bash
# Mint the next ALPHA-YY-MM-DD-NN milestone tag (annotated, on HEAD).
# NN = highest existing NN for today + 1 (two digits, grows to three past
# 99). NOTE: deleting the day's NEWEST tag frees its number for reuse —
# never delete a tag that shipped. Milestone tags are created ONLY when
# Matthew declares one.
# Beta transition: change PREFIX here + the --match pattern in the top-level
# CMakeLists.txt + the regexes in tests/test_oap_version.cpp and the CMake
# validation (see AGENTS.md § Versioning).
set -euo pipefail

PREFIX="ALPHA"
TODAY="$(date +%y-%m-%d)"
# Numeric suffixes only, so a malformed hand-made tag can't break the
# arithmetic; grep exits 1 on no match, hence || true.
LAST="$(git tag --list "${PREFIX}-${TODAY}-*" \
        | sed "s/^${PREFIX}-${TODAY}-//" \
        | grep -E '^[0-9]+$' | sort -n | tail -n1 || true)"
# 10#: NN is zero-padded — force base-10 so 08/09 don't parse as octal.
NN="$(printf '%02d' $(( 10#${LAST:-0} + 1 )))"
TAG="${PREFIX}-${TODAY}-${NN}"

# Tracked changes only — same semantics as `git describe --dirty`.
if ! git diff-index --quiet HEAD --; then
    echo "WARNING: working tree is dirty — a build from this tree reports -dirty." >&2
fi

git tag -a "$TAG" -m "Milestone build ${TAG}"
echo "Created tag: ${TAG}"
echo "Next steps:"
echo "  1. Push the tag (needs go-ahead): git push origin ${TAG}"
echo "  2. Reconfigure + rebuild — the version is captured at CONFIGURE time."
echo "  3. Build the Pi release (official tags always ship one):"
echo "       ./cross-build.sh"
echo "       tools/package-prebuilt-release.sh --build-dir build-pi --output-dir dist --version-tag ${TAG}"
echo "  4. Publish: gh release create ${TAG} dist/openauto-prodigy-prebuilt-${TAG}-pi4-aarch64.tar.gz --prerelease"
