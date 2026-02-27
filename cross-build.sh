#!/bin/bash
# Cross-compile openauto-prodigy for Raspberry Pi 4 (aarch64) using Docker
# Usage: ./cross-build.sh [cmake args...]
# Example: ./cross-build.sh -DCMAKE_BUILD_TYPE=Release

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMAGE_NAME="openauto-cross-pi4"

# Build Docker image if it doesn't exist
DOCKERFILE="$SCRIPT_DIR/docker/Dockerfile.cross-pi4"
if ! docker image inspect "$IMAGE_NAME" &>/dev/null; then
    echo "==> Building cross-compilation Docker image (one-time)..."
    docker build -t "$IMAGE_NAME" -f "$DOCKERFILE" "$SCRIPT_DIR/docker"
fi

echo "==> Cross-compiling for Pi 4 (aarch64)..."
docker run --rm \
    -u "$(id -u):$(id -g)" \
    -v "$SCRIPT_DIR:/src:rw" \
    -w /src \
    "$IMAGE_NAME" \
    bash -c "
        mkdir -p build-pi && cd build-pi && \
        cmake -DCMAKE_TOOLCHAIN_FILE=../docker/toolchain-pi4-docker.cmake $* .. && \
        cmake --build . -j\$(nproc)
    "

echo "==> Build complete: build-pi/src/openauto-prodigy"
echo "    Deploy: rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/"
