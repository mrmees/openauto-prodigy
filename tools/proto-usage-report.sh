#!/usr/bin/env bash
# Proto usage report — shows which open-android-auto protos are used by openauto-prodigy
# Run from the repo root: ./tools/proto-usage-report.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROTO_DIR="$REPO_ROOT/libs/open-androidauto/proto"
SRC_DIRS=("$REPO_ROOT/src" "$REPO_ROOT/libs/open-androidauto/src" "$REPO_ROOT/libs/open-androidauto/include" "$REPO_ROOT/tests")

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Collect all proto files in the submodule (relative paths like oaa/audio/Foo)
mapfile -t ALL_PROTOS < <(find "$PROTO_DIR" -name '*.proto' -printf '%P\n' | sed 's/\.proto$//' | sort)

# Collect all .pb.h includes from source (same relative path format)
mapfile -t USED_PROTOS < <(
    grep -roh '"oaa/[^"]*\.pb\.h"' "${SRC_DIRS[@]}" 2>/dev/null \
    | sed 's/^"//;s/\.pb\.h"$//' \
    | sort -u
)

# Build lookup set
declare -A USED_SET
for p in "${USED_PROTOS[@]}"; do
    USED_SET["$p"]=1
done

# Group by directory
declare -A DIR_TOTAL
declare -A DIR_USED
for p in "${ALL_PROTOS[@]}"; do
    dir=$(dirname "$p")
    DIR_TOTAL["$dir"]=$(( ${DIR_TOTAL["$dir"]:-0} + 1 ))
done
for p in "${USED_PROTOS[@]}"; do
    dir=$(dirname "$p")
    DIR_USED["$dir"]=$(( ${DIR_USED["$dir"]:-0} + 1 ))
done

mode="${1:-summary}"

case "$mode" in
    summary)
        echo -e "${CYAN}Proto Usage Summary${NC}"
        echo -e "Total protos in submodule: ${#ALL_PROTOS[@]}"
        echo -e "Used by openauto-prodigy:  ${GREEN}${#USED_PROTOS[@]}${NC}"
        echo -e "Unused:                    $((${#ALL_PROTOS[@]} - ${#USED_PROTOS[@]}))"
        echo ""
        echo -e "${CYAN}By directory:${NC}"
        printf "  %-50s %s\n" "Directory" "Used / Total"
        printf "  %-50s %s\n" "---------" "------------"
        for dir in $(echo "${!DIR_TOTAL[@]}" | tr ' ' '\n' | sort); do
            used=${DIR_USED["$dir"]:-0}
            total=${DIR_TOTAL["$dir"]}
            if [[ $used -gt 0 ]]; then
                printf "  %-50s ${GREEN}%3d${NC} / %d\n" "$dir" "$used" "$total"
            else
                printf "  %-50s ${RED}%3d${NC} / %d\n" "$dir" "$used" "$total"
            fi
        done
        ;;

    used)
        echo -e "${GREEN}Used protos (${#USED_PROTOS[@]}):${NC}"
        for p in "${USED_PROTOS[@]}"; do
            echo "  $p"
        done
        ;;

    unused)
        echo -e "${YELLOW}Unused protos:${NC}"
        for p in "${ALL_PROTOS[@]}"; do
            if [[ -z "${USED_SET["$p"]:-}" ]]; then
                echo "  $p"
            fi
        done
        ;;

    check)
        # Check for changes to used protos since a given git ref (default: HEAD~1)
        ref="${2:-HEAD~1}"
        echo -e "${CYAN}Checking for changes to used protos since $ref${NC}"
        changed=0
        pushd "$REPO_ROOT/libs/open-androidauto" > /dev/null
        for p in "${USED_PROTOS[@]}"; do
            proto_file="proto/${p}.proto"
            if git diff --quiet "$ref" -- "$proto_file" 2>/dev/null; then
                continue
            fi
            if [[ $changed -eq 0 ]]; then
                echo -e "${YELLOW}Changed protos we depend on:${NC}"
            fi
            echo -e "  ${RED}$p${NC}"
            git diff --stat "$ref" -- "$proto_file" 2>/dev/null | sed 's/^/    /'
            changed=$((changed + 1))
        done
        popd > /dev/null
        if [[ $changed -eq 0 ]]; then
            echo -e "${GREEN}No changes to used protos.${NC}"
        else
            echo ""
            echo -e "${YELLOW}$changed proto(s) we use have changed. Review before updating submodule.${NC}"
        fi
        ;;

    where)
        # Show where each proto is used
        proto_filter="${2:-}"
        echo -e "${CYAN}Proto usage locations:${NC}"
        for p in "${USED_PROTOS[@]}"; do
            if [[ -n "$proto_filter" && "$p" != *"$proto_filter"* ]]; then continue; fi
            echo -e "\n${GREEN}$p${NC}"
            basename_pb=$(basename "$p")
            grep -rn "${basename_pb}.pb.h" "${SRC_DIRS[@]}" 2>/dev/null | sed 's/^/  /'
        done
        ;;

    *)
        echo "Usage: $0 [summary|used|unused|check [git-ref]|where [filter]]"
        echo ""
        echo "  summary  - Overview of proto usage by directory (default)"
        echo "  used     - List all protos we depend on"
        echo "  unused   - List all protos we don't use"
        echo "  check    - Show changes to used protos since a git ref"
        echo "  where    - Show which source files include each proto"
        exit 1
        ;;
esac
