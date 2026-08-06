#!/usr/bin/env bash
# Initialize the protocol submodule from its lightweight distribution branch
# while preserving the exact gitlink commit pinned by the superproject.
set -euo pipefail

INSTALL_ROOT="$(readlink -f -- "${1:?usage: initialize-protocol-submodule.sh INSTALL_ROOT}")"
RELATIVE_PATH="libs/prodigy-oaa-protocol/proto"
CHECKOUT_PATH="$INSTALL_ROOT/$RELATIVE_PATH"

cd "$INSTALL_ROOT"

git submodule init -- "$RELATIVE_PATH"
URL="$(git config --get "submodule.$RELATIVE_PATH.url")"
BRANCH="$(git config -f .gitmodules --get "submodule.$RELATIVE_PATH.branch")"
PIN="$(git rev-parse "HEAD:$RELATIVE_PATH")"
GIT_DIR="$(git rev-parse --git-path "modules/$RELATIVE_PATH")"

if ! git -C "$CHECKOUT_PATH" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    mkdir -p "$(dirname "$GIT_DIR")"
    git clone --no-checkout --single-branch --branch "$BRANCH" \
        --separate-git-dir "$GIT_DIR" "$URL" "$CHECKOUT_PATH"
fi

git -C "$CHECKOUT_PATH" remote set-url origin "$URL"
git -C "$CHECKOUT_PATH" config remote.origin.fetch \
    "+refs/heads/$BRANCH:refs/remotes/origin/$BRANCH"
if ! git -C "$CHECKOUT_PATH" cat-file -e "${PIN}^{commit}" 2>/dev/null; then
    git -C "$CHECKOUT_PATH" fetch origin "$BRANCH"
fi
git -C "$CHECKOUT_PATH" checkout --detach "$PIN"
git submodule absorbgitdirs -- "$RELATIVE_PATH"
