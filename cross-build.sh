#!/bin/bash
# Cross-compile openauto-prodigy for Raspberry Pi 4 (aarch64) using Docker
# Usage: ./cross-build.sh [--full] [--reset-cache] [cmake args...]
# Example: ./cross-build.sh -DCMAKE_BUILD_TYPE=Release
#
# Default (fast) mode builds only the app target (openauto-prodigy) — that's
# all a Pi deploy ever needs. Pass --full to build every target, including the
# ARM test binaries (each links the large static core library).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
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

BUILD_JOBS="${CROSS_BUILD_JOBS:-$(nproc)}"
HOST_UID="$(id -u)"
HOST_GID="$(id -g)"
REPO_CACHE_KEY="$(printf '%s' "$SCRIPT_DIR" | sha256sum | cut -c1-12)"
LOCK_BASE_DIR="${XDG_RUNTIME_DIR:-/tmp}"
if [[ ! -d "$LOCK_BASE_DIR" ]]; then
    echo "Cross-build lock base directory does not exist: $LOCK_BASE_DIR" >&2
    exit 1
fi
CROSS_BUILD_LOCK_DIR="$LOCK_BASE_DIR/openauto-prodigy-cross-build-$HOST_UID"
if [[ -L "$CROSS_BUILD_LOCK_DIR" ]]; then
    echo "Cross-build lock directory must not be a symlink: $CROSS_BUILD_LOCK_DIR" >&2
    exit 1
fi
if [[ ! -e "$CROSS_BUILD_LOCK_DIR" ]]; then
    (umask 077 && mkdir -- "$CROSS_BUILD_LOCK_DIR")
fi
if [[ ! -d "$CROSS_BUILD_LOCK_DIR" ]]; then
    echo "Cross-build lock path is not a directory: $CROSS_BUILD_LOCK_DIR" >&2
    exit 1
fi
LOCK_DIR_UID="$(stat -c '%u' -- "$CROSS_BUILD_LOCK_DIR")"
LOCK_DIR_MODE="$(stat -c '%a' -- "$CROSS_BUILD_LOCK_DIR")"
if [[ "$LOCK_DIR_UID" != "$HOST_UID" || "$LOCK_DIR_MODE" != "700" ]]; then
    echo "Cross-build lock directory must be owned by UID $HOST_UID with mode 700: $CROSS_BUILD_LOCK_DIR" >&2
    exit 1
fi

# Fast and full modes both publish build-pi/src/openauto-prodigy. Serialize the
# entire checkout workflow so they cannot race while replacing that artifact.
exec 9>"$CROSS_BUILD_LOCK_DIR/openauto-prodigy-cross-checkout-$REPO_CACHE_KEY.lock"
flock 9

ACTIVE_CONTAINER=""
cleanup_active_container() {
    if [[ -n "$ACTIVE_CONTAINER" ]]; then
        docker rm -f "$ACTIVE_CONTAINER" >/dev/null 2>&1 || true
    fi
}
trap cleanup_active_container EXIT
trap 'exit 130' HUP INT TERM

remove_stale_container() {
    docker rm -f "$1" >/dev/null 2>&1 || true
}

run_owned_container() {
    local container_name="$1"
    shift
    remove_stale_container "$container_name"
    ACTIVE_CONTAINER="$container_name"
    docker run --name "$container_name" --rm "$@"
    ACTIVE_CONTAINER=""
}

# Build Docker image if it doesn't exist
DOCKERFILE="$SCRIPT_DIR/docker/Dockerfile.cross-pi4"
if ! docker image inspect "$IMAGE_NAME" &>/dev/null; then
    echo "==> Building cross-compilation Docker image (one-time)..."
    docker build -t "$IMAGE_NAME" -f "$DOCKERFILE" "$SCRIPT_DIR/docker"
fi

echo "==> Cross-compiling for Pi 4 (aarch64)..."
CHECKOUT_CONTAINER_PREFIX="openauto-prodigy-pi4-$HOST_UID-$REPO_CACHE_KEY"
remove_stale_container "$CHECKOUT_CONTAINER_PREFIX-full-configure"
remove_stale_container "$CHECKOUT_CONTAINER_PREFIX-full-build"
remove_stale_container "$CHECKOUT_CONTAINER_PREFIX-publish"

if [[ "$FULL_BUILD" -eq 1 ]]; then
    if [[ "$RESET_CACHE" -eq 1 ]]; then
        echo "--reset-cache applies only to the app-only Docker-volume cache" >&2
        exit 1
    fi

    echo "Full mode: host-visible ARM test binaries; intermediates remain in build-pi"
    mkdir -p "$SCRIPT_DIR/build-pi"
    run_owned_container "$CHECKOUT_CONTAINER_PREFIX-full-configure" \
        -u "$HOST_UID:$HOST_GID" \
        -v "$SCRIPT_DIR:/src:rw" \
        "$IMAGE_NAME" \
        cmake -S /src -B /src/build-pi \
            -DCMAKE_TOOLCHAIN_FILE=/src/docker/toolchain-pi4-docker.cmake \
            "${CMAKE_ARGS[@]}"
    run_owned_container "$CHECKOUT_CONTAINER_PREFIX-full-build" \
        -u "$HOST_UID:$HOST_GID" \
        -v "$SCRIPT_DIR:/src:rw" \
        "$IMAGE_NAME" \
        cmake --build /src/build-pi "-j$BUILD_JOBS"
else
    # Docker named volumes live on the Linux filesystem inside Docker's WSL2
    # VM. Keep high-churn CMake/object files there; the Windows/9p source mount
    # stays read-only and receives only the final deploy/package binary.
    BUILD_VOLUME="${CROSS_BUILD_VOLUME:-openauto-prodigy-pi4-build-$HOST_UID-$REPO_CACHE_KEY}"
    echo "Fast mode: app target only; cache volume: $BUILD_VOLUME"

    # A named volume is a single mutable CMake tree. Serialize users of the
    # same volume even when an override shares it across checkouts.
    VOLUME_LOCK_KEY="$(printf '%s' "$BUILD_VOLUME" | sha256sum | cut -c1-12)"
    exec 8>"$CROSS_BUILD_LOCK_DIR/openauto-prodigy-cross-volume-$VOLUME_LOCK_KEY.lock"
    flock 8
    CACHE_CONTAINER_PREFIX="openauto-prodigy-pi4-cache-$HOST_UID-$VOLUME_LOCK_KEY"
    remove_stale_container "$CACHE_CONTAINER_PREFIX-chown"
    remove_stale_container "$CACHE_CONTAINER_PREFIX-configure"
    remove_stale_container "$CACHE_CONTAINER_PREFIX-build"

    if [[ "$RESET_CACHE" -eq 1 ]] && docker volume inspect "$BUILD_VOLUME" &>/dev/null; then
        docker volume rm "$BUILD_VOLUME" >/dev/null
    fi
    if ! docker volume inspect "$BUILD_VOLUME" &>/dev/null; then
        docker volume create "$BUILD_VOLUME" >/dev/null
    fi
    # Repair interrupted/legacy cache initialization on every run. Docker can
    # leave a newly created volume root-owned if the first chown container is
    # interrupted; the idempotent repair avoids a permanently wedged cache.
    run_owned_container "$CACHE_CONTAINER_PREFIX-chown" \
        -v "$BUILD_VOLUME:/build" \
        "$IMAGE_NAME" \
        chown "$HOST_UID:$HOST_GID" /build

    run_owned_container "$CACHE_CONTAINER_PREFIX-configure" \
        -u "$HOST_UID:$HOST_GID" \
        -v "$SCRIPT_DIR:/src:ro" \
        -v "$BUILD_VOLUME:/build:rw" \
        "$IMAGE_NAME" \
        cmake -S /src -B /build \
            -DCMAKE_TOOLCHAIN_FILE=/src/docker/toolchain-pi4-docker.cmake \
            "${CMAKE_ARGS[@]}"
    run_owned_container "$CACHE_CONTAINER_PREFIX-build" \
        -u "$HOST_UID:$HOST_GID" \
        -v "$SCRIPT_DIR:/src:ro" \
        -v "$BUILD_VOLUME:/build:rw" \
        "$IMAGE_NAME" \
        cmake --build /build "-j$BUILD_JOBS" --target openauto-prodigy

    mkdir -p "$SCRIPT_DIR/build-pi/src"
    run_owned_container "$CHECKOUT_CONTAINER_PREFIX-publish" \
        -u "$HOST_UID:$HOST_GID" \
        -v "$BUILD_VOLUME:/build:ro" \
        -v "$SCRIPT_DIR/build-pi:/out:rw" \
        "$IMAGE_NAME" \
        install -m 0755 /build/src/openauto-prodigy /out/src/openauto-prodigy
fi

echo "==> Build complete: build-pi/src/openauto-prodigy"
echo "    Deploy: rsync -av build-pi/src/openauto-prodigy matt@192.168.1.149:~/openauto-prodigy/build/src/"
