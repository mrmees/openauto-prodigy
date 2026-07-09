#!/usr/bin/env bash
# Regenerates resources/web/prodigy-proto.js (+ vendored protobuf.min.js)
# from proto/api/*.proto. Requires Node.js — needed ONLY when the frozen
# additive proto contract gains fields; never part of the CMake build
# (design 2026-07-06-js-runtime D7).
set -euo pipefail
cd "$(dirname "$0")/.."

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

npm install --prefix "$WORK" --no-save --silent protobufjs@7 protobufjs-cli@1

"$WORK/node_modules/.bin/pbjs" \
    -t static-module -w closure -r prodigy-api \
    --no-service --no-delimited \
    -p proto \
    -o resources/web/prodigy-proto.js \
    proto/api/*.proto

cp "$WORK/node_modules/protobufjs/dist/minimal/protobuf.min.js" \
    resources/web/protobuf.min.js

echo "Regenerated resources/web/prodigy-proto.js and protobuf.min.js"
echo "Root: protobuf.roots['prodigy-api'].prodigy.api.v1"
