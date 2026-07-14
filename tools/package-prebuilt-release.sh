#!/usr/bin/env bash
#
# Package a prebuilt OpenAuto Prodigy release tarball for Raspberry Pi.
#
# Example:
#   ./tools/package-prebuilt-release.sh \
#     --build-dir build-pi \
#     --output-dir dist \
#     --version-tag ALPHA-26-07-09-01
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="$REPO_ROOT/build-pi"
OUTPUT_DIR="$REPO_ROOT/dist"
VERSION_TAG="$(date -u +%Y%m%d-%H%M%S)"
TARGET_NAME="pi4-aarch64"

usage() {
    cat <<'USAGE'
Usage: package-prebuilt-release.sh [options]

Options:
  --build-dir <path>    Build directory containing src/openauto-prodigy
                        (default: ./build-pi)
  --output-dir <path>   Directory for output tarball and staging
                        (default: ./dist)
  --version-tag <tag>   Release tag suffix for archive naming
                        (default: UTC timestamp YYYYMMDD-HHMMSS)
  --target <name>       Target suffix used in asset naming
                        (default: pi4-aarch64)
  --help                Show this help text
USAGE
}

fail() {
    echo "[FAIL] $*" >&2
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --version-tag)
            VERSION_TAG="$2"
            shift 2
            ;;
        --target)
            TARGET_NAME="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            fail "unknown argument: $1"
            ;;
    esac
done

[[ -n "$VERSION_TAG" ]] || fail "version tag cannot be empty"
[[ -n "$TARGET_NAME" ]] || fail "target name cannot be empty"

if [[ "$BUILD_DIR" != /* ]]; then
    BUILD_DIR="$REPO_ROOT/$BUILD_DIR"
fi
if [[ "$OUTPUT_DIR" != /* ]]; then
    OUTPUT_DIR="$REPO_ROOT/$OUTPUT_DIR"
fi

BINARY_PATH="$BUILD_DIR/src/openauto-prodigy"
INSTALL_SCRIPT="$REPO_ROOT/install-prebuilt.sh"
RESTART_SCRIPT="$REPO_ROOT/docs/pi-config/restart.sh"

[[ -x "$BINARY_PATH" ]] || fail "missing executable binary: $BINARY_PATH"
[[ -f "$INSTALL_SCRIPT" ]] || fail "missing installer script: $INSTALL_SCRIPT"
[[ -f "$RESTART_SCRIPT" ]] || fail "missing restart helper: $RESTART_SCRIPT"

PACKAGE_NAME="openauto-prodigy-prebuilt-${VERSION_TAG}-${TARGET_NAME}"
STAGE_DIR="$OUTPUT_DIR/$PACKAGE_NAME"
ARCHIVE_PATH="$OUTPUT_DIR/${PACKAGE_NAME}.tar.gz"
GIT_COMMIT="$(git -C "$REPO_ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"

echo "[INFO] Packaging prebuilt release"
echo "       build:   $BUILD_DIR"
echo "       output:  $ARCHIVE_PATH"

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR/payload/build/src"

# Release entrypoint
cp "$INSTALL_SCRIPT" "$STAGE_DIR/install-prebuilt.sh"
chmod +x "$STAGE_DIR/install-prebuilt.sh"

cat > "$STAGE_DIR/RELEASE.json" <<JSON
{
  "project": "openauto-prodigy",
  "version_tag": "$VERSION_TAG",
  "target": "$TARGET_NAME",
  "asset_name": "${PACKAGE_NAME}.tar.gz",
  "git_commit": "$GIT_COMMIT",
  "created_utc": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
JSON

# Runtime payload
cp "$BINARY_PATH" "$STAGE_DIR/payload/build/src/openauto-prodigy"
chmod +x "$STAGE_DIR/payload/build/src/openauto-prodigy"

cp -a "$REPO_ROOT/config" "$STAGE_DIR/payload/"
cp -a "$REPO_ROOT/system-service" "$STAGE_DIR/payload/"
cp -a "$REPO_ROOT/web-config" "$STAGE_DIR/payload/"
cp "$RESTART_SCRIPT" "$STAGE_DIR/payload/restart.sh"
chmod +x "$STAGE_DIR/payload/restart.sh"

# HFP mSBC codec fix: ship the patched libspa-0.2-bluetooth deb when one has
# been built (tools/pipewire-msbc/build.sh — version-locked to the target's
# pipewire base, so it is NOT in git). Without it a fresh prebuilt install
# warns and calls risk LC3-SWB's silent uplink until the deb is provided.
MSBC_DEB="$(ls "$REPO_ROOT/tools/pipewire-msbc/out"/libspa-0.2-bluetooth_*+prodigy*_arm64.deb 2>/dev/null | head -1 || true)"
if [[ -n "$MSBC_DEB" ]]; then
    mkdir -p "$STAGE_DIR/payload/pipewire-msbc"
    cp "$MSBC_DEB" "$STAGE_DIR/payload/pipewire-msbc/"
    echo "[OK] staged HFP mSBC codec fix: $(basename "$MSBC_DEB")"
else
    echo "[WARN] no patched libspa-0.2-bluetooth deb in tools/pipewire-msbc/out/ —" >&2
    echo "[WARN] this release will install WITHOUT the HFP mSBC codec fix" >&2
    echo "[WARN] (LC3-SWB uplink is silent; see tools/pipewire-msbc/README.md)" >&2
fi

# Keep package size focused on runtime content only
rm -rf \
    "$STAGE_DIR/payload/system-service/.pytest_cache" \
    "$STAGE_DIR/payload/system-service/__pycache__"

mkdir -p "$OUTPUT_DIR"
rm -f "$ARCHIVE_PATH"
tar -C "$OUTPUT_DIR" -czf "$ARCHIVE_PATH" "$PACKAGE_NAME"

echo "[OK] Created $ARCHIVE_PATH"
