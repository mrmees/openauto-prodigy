#!/bin/bash
# Cross-compile openauto-prodigy for Raspberry Pi 4 (aarch64) using Docker
# Usage: ./cross-build.sh [--full] [cmake args...]
# Example: ./cross-build.sh -DCMAKE_BUILD_TYPE=Release
#
# Default (fast) mode builds only the app target (openauto-prodigy) — that's
# all a Pi deploy ever needs. Pass --full to build every target, including the
# ~30 ARM test binaries (each links a ~250MB static lib; ~20 min vs ~4-6 min).

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMAGE_NAME="openauto-cross-pi4"

FULL_BUILD=0
CMAKE_ARGS=()
for arg in "$@"; do
    if [[ "$arg" == "--full" ]]; then
        FULL_BUILD=1
    else
        CMAKE_ARGS+=("$arg")
    fi
done

# Build Docker image if it doesn't exist
DOCKERFILE="$SCRIPT_DIR/docker/Dockerfile.cross-pi4"
if ! docker image inspect "$IMAGE_NAME" &>/dev/null; then
    echo "==> Building cross-compilation Docker image (one-time)..."
    docker build -t "$IMAGE_NAME" -f "$DOCKERFILE" "$SCRIPT_DIR/docker"
fi

if [[ "$FULL_BUILD" -eq 1 ]]; then
    BUILD_CMD="cmake --build . -j\$(nproc)"
else
    echo "Fast mode: building app target only (use --full for all targets incl. ARM test binaries)"
    BUILD_CMD="cmake --build . -j\$(nproc) --target openauto-prodigy"
fi

echo "==> Cross-compiling for Pi 4 (aarch64)..."
docker run --rm \
    -u "$(id -u):$(id -g)" \
    -v "$SCRIPT_DIR:/src:rw" \
    -w /src \
    "$IMAGE_NAME" \
    bash -c "
        mkdir -p build-pi && cd build-pi && \
        cmake -DCMAKE_TOOLCHAIN_FILE=../docker/toolchain-pi4-docker.cmake ${CMAKE_ARGS[*]} .. && \
        $BUILD_CMD
    "

echo "==> Build complete: build-pi/src/openauto-prodigy"
echo "    Deploy: rsync -av build-pi/src/openauto-prodigy matt@192.168.1.149:~/openauto-prodigy/build/src/"
