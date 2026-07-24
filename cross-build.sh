#!/bin/bash
# Cross-compile openauto-prodigy for Raspberry Pi 4 (aarch64) using Docker
# Usage: ./cross-build.sh [--full] [--reset-cache] [cmake args...]
# Example: ./cross-build.sh -DCMAKE_BUILD_TYPE=Release
#
# Default (fast) mode builds only the app target (openauto-prodigy) — that's
# all a Pi deploy ever needs. Pass --full to build every target, including the
# ARM test binaries (each links the large static core library).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMAGE_NAME="openauto-cross-pi4"

FULL_BUILD=0
RESET_CACHE=0
CMAKE_ARGS=()
for arg in "$@"; do
    if [[ "$arg" == "--full" ]]; then
        FULL_BUILD=1
    elif [[ "$arg" == "--reset-cache" ]]; then
        RESET_CACHE=1
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

echo "==> Cross-compiling for Pi 4 (aarch64)..."
BUILD_JOBS="${CROSS_BUILD_JOBS:-$(nproc)}"
HOST_UID="$(id -u)"
HOST_GID="$(id -g)"

if [[ "$FULL_BUILD" -eq 1 ]]; then
    if [[ "$RESET_CACHE" -eq 1 ]]; then
        echo "--reset-cache applies only to the app-only Docker-volume cache" >&2
        exit 1
    fi

    echo "Full mode: host-visible ARM test binaries; intermediates remain in build-pi"
    mkdir -p "$SCRIPT_DIR/build-pi"
    docker run --rm \
        -u "$HOST_UID:$HOST_GID" \
        -v "$SCRIPT_DIR:/src:rw" \
        "$IMAGE_NAME" \
        cmake -S /src -B /src/build-pi \
            -DCMAKE_TOOLCHAIN_FILE=/src/docker/toolchain-pi4-docker.cmake \
            "${CMAKE_ARGS[@]}"
    docker run --rm \
        -u "$HOST_UID:$HOST_GID" \
        -v "$SCRIPT_DIR:/src:rw" \
        "$IMAGE_NAME" \
        cmake --build /src/build-pi "-j$BUILD_JOBS"
else
    # Docker named volumes live on the Linux filesystem inside Docker's WSL2
    # VM. Keep high-churn CMake/object files there; the Windows/9p source mount
    # stays read-only and receives only the final deploy/package binary.
    BUILD_VOLUME="${CROSS_BUILD_VOLUME:-openauto-prodigy-pi4-build-$HOST_UID}"
    echo "Fast mode: app target only; cache volume: $BUILD_VOLUME"

    if [[ "$RESET_CACHE" -eq 1 ]] && docker volume inspect "$BUILD_VOLUME" &>/dev/null; then
        docker volume rm "$BUILD_VOLUME" >/dev/null
    fi
    if ! docker volume inspect "$BUILD_VOLUME" &>/dev/null; then
        docker volume create "$BUILD_VOLUME" >/dev/null
        docker run --rm \
            -v "$BUILD_VOLUME:/build" \
            "$IMAGE_NAME" \
            chown "$HOST_UID:$HOST_GID" /build
    fi

    docker run --rm \
        -u "$HOST_UID:$HOST_GID" \
        -v "$SCRIPT_DIR:/src:ro" \
        -v "$BUILD_VOLUME:/build:rw" \
        "$IMAGE_NAME" \
        cmake -S /src -B /build \
            -DCMAKE_TOOLCHAIN_FILE=/src/docker/toolchain-pi4-docker.cmake \
            "${CMAKE_ARGS[@]}"
    docker run --rm \
        -u "$HOST_UID:$HOST_GID" \
        -v "$SCRIPT_DIR:/src:ro" \
        -v "$BUILD_VOLUME:/build:rw" \
        "$IMAGE_NAME" \
        cmake --build /build "-j$BUILD_JOBS" --target openauto-prodigy

    mkdir -p "$SCRIPT_DIR/build-pi/src"
    docker run --rm \
        -u "$HOST_UID:$HOST_GID" \
        -v "$BUILD_VOLUME:/build:ro" \
        -v "$SCRIPT_DIR/build-pi:/out:rw" \
        "$IMAGE_NAME" \
        install -m 0755 /build/src/openauto-prodigy /out/src/openauto-prodigy
fi

echo "==> Build complete: build-pi/src/openauto-prodigy"
echo "    Deploy: rsync -av build-pi/src/openauto-prodigy matt@192.168.1.149:~/openauto-prodigy/build/src/"
