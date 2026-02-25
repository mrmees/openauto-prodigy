#!/usr/bin/env python3
"""Validate our proto definitions against decompiled Android Auto APK descriptors."""

from __future__ import annotations

import json
import os
import sys
from datetime import date
from pathlib import Path

TOOLS_DIR = Path(__file__).parent
sys.path.insert(0, str(TOOLS_DIR))

from descriptor_decoder import decode_class
from match_loader import load_matches
from proto_comparator import compare_fields
from proto_parser import parse_all_protos

PROTO_DIR = TOOLS_DIR.parent / "libs" / "open-androidauto" / "proto"
REPORT_JSON = TOOLS_DIR / "proto_validation_report.json"
REPORT_MD = TOOLS_DIR.parent / "docs" / "proto-validation-report.md"


def validate_all(filter_message: str | None = None) -> list[dict]:
    """Run the full validation pipeline."""
    our_protos = parse_all_protos(str(PROTO_DIR))
    print(f"Parsed {len(our_protos)} messages from {PROTO_DIR}")

    matches = load_matches()
    print(f"Loaded {len(matches)} APK class matches")

    results: list[dict] = []
    for msg_name, msg_data in sorted(our_protos.items()):
        if filter_message and msg_name != filter_message:
            continue

        match = matches.get(msg_name)
        if not match:
            results.append(
                {
                    "message": msg_name,
                    "file": msg_data["file"],
                    "status": "unmatched",
                    "apk_class": None,
                    "match_source": None,
                    "match_score": 0,
                    "comparison": None,
                }
            )
            continue

        apk_class = match["apk_class"]
        decoded = decode_class(apk_class)
        if not decoded:
            results.append(
                {
                    "message": msg_name,
                    "file": msg_data["file"],
                    "status": "error",
                    "apk_class": apk_class,
                    "match_source": match["source"],
                    "match_score": match["score"],
                    "comparison": None,
                    "error": f"Failed to decode APK class {apk_class}",
                }
            )
            continue

        comparison = compare_fields(msg_data["fields"], decoded["fields"])
        results.append(
            {
                "message": msg_name,
                "file": msg_data["file"],
                "status": comparison["status"],
                "apk_class": apk_class,
                "match_source": match["source"],
                "match_score": match["score"],
                "comparison": comparison,
            }
        )

    return results


def generate_json_report(results: list[dict]) -> None:
    """Write machine-readable JSON report."""
    summary = {
        "date": str(date.today()),
        "total": len(results),
        "verified": sum(1 for row in results if row["status"] == "verified"),
        "partial": sum(1 for row in results if row["status"] == "partial"),
        "unmatched": sum(1 for row in results if row["status"] == "unmatched"),
        "error": sum(1 for row in results if row["status"] == "error"),
    }
    REPORT_JSON.write_text(json.dumps({"summary": summary, "results": results}, indent=2), encoding="utf-8")
    print(f"JSON report: {REPORT_JSON}")


def generate_md_report(results: list[dict]) -> None:
    """Write human-readable Markdown report."""
    verified = [row for row in results if row["status"] == "verified"]
    partial = [row for row in results if row["status"] == "partial"]
    unmatched = [row for row in results if row["status"] == "unmatched"]
    errors = [row for row in results if row["status"] == "error"]

    lines = [
        "# Proto Validation Report",
        "",
        f"Generated: {date.today()}",
        "",
        "## Summary",
        "",
        "| Status | Count |",
        "|--------|-------|",
        f"| Verified | {len(verified)} |",
        f"| Partial (diffs) | {len(partial)} |",
        f"| Unmatched | {len(unmatched)} |",
        f"| Error | {len(errors)} |",
        f"| **Total** | **{len(results)}** |",
        "",
    ]

    if verified:
        lines.append("## Verified (all fields match)")
        lines.append("")
        for row in verified:
            src = f" [{row['match_source']}]" if row["match_source"] else ""
            lines.append(f"- **{row['message']}** ({row['file']}) -> `{row['apk_class']}`{src}")
        lines.append("")

    if partial:
        lines.append("## Partial (field diffs)")
        lines.append("")
        for row in partial:
            comp = row["comparison"]
            src = f" [{row['match_source']}]" if row["match_source"] else ""
            lines.append(f"### {row['message']} ({row['file']}) -> `{row['apk_class']}`{src}")
            lines.append("")
            lines.append(
                f"Matched: {comp['matched_count']}/{comp['our_field_count']} our fields, "
                f"APK has {comp['apk_field_count']} fields"
            )
            lines.append("")
            if comp["mismatches"]:
                lines.append("| Field | Issue | Ours | APK |")
                lines.append("|-------|-------|------|-----|")
                for mismatch in comp["mismatches"]:
                    lines.append(
                        f"| {mismatch['field']} | {mismatch['issue']} | "
                        f"{mismatch['our_value']} | {mismatch['apk_value']} |"
                    )
                lines.append("")

    if unmatched:
        lines.append("## Unmatched (no APK class found)")
        lines.append("")
        for row in unmatched:
            lines.append(f"- {row['message']} ({row['file']})")
        lines.append("")

    if errors:
        lines.append("## Errors")
        lines.append("")
        for row in errors:
            lines.append(f"- {row['message']} -> `{row['apk_class']}`: {row.get('error', 'unknown')}")
        lines.append("")

    REPORT_MD.write_text("\n".join(lines), encoding="utf-8")
    print(f"Markdown report: {REPORT_MD}")


def print_summary(results: list[dict]) -> None:
    """Print validation summary to stdout."""
    verified = sum(1 for row in results if row["status"] == "verified")
    partial_count = sum(1 for row in results if row["status"] == "partial")
    unmatched = sum(1 for row in results if row["status"] == "unmatched")
    errors = sum(1 for row in results if row["status"] == "error")

    print(f"\n{'=' * 50}")
    print("Proto Validation Results")
    print(f"{'=' * 50}")
    print(f"  Verified:  {verified}")
    print(f"  Partial:   {partial_count}")
    print(f"  Unmatched: {unmatched}")
    print(f"  Errors:    {errors}")
    print(f"  Total:     {len(results)}")
    print(f"{'=' * 50}")

    partial_rows = [row for row in results if row["status"] == "partial"]
    if partial_rows:
        print("\nPartial matches (need review):")
        for row in partial_rows:
            comp = row["comparison"]
            issues = ", ".join(f"f{m['field']}:{m['issue']}" for m in comp["mismatches"][:3])
            more = f" +{len(comp['mismatches']) - 3} more" if len(comp["mismatches"]) > 3 else ""
            print(f"  {row['message']} -> {row['apk_class']}: {issues}{more}")


def main() -> None:
    args = sys.argv[1:]
    filter_msg = None
    summary_only = False

    if "--message" in args:
        idx = args.index("--message")
        filter_msg = args[idx + 1] if idx + 1 < len(args) else None
    if "--summary" in args:
        summary_only = True

    results = validate_all(filter_message=filter_msg)
    print_summary(results)

    if not summary_only:
        generate_json_report(results)
        generate_md_report(results)


if __name__ == "__main__":
    main()
