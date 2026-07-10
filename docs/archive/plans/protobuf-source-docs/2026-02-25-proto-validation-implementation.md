# Proto Validation Pipeline — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a rerunnable Python tool that cross-references our 150 proto files against the decompiled AA APK v16.1 to verify field numbers, types, and cardinality match.

**Architecture:** Three-stage pipeline: (1) parse our .proto files into structured data, (2) load APK decoder results and match data, (3) diff field-by-field and produce a report. Uses existing `descriptor_decoder.py` as a library import.

**Tech Stack:** Python 3, no external dependencies. Reads proto2 files via regex, imports `descriptor_decoder` module, outputs JSON + Markdown.

---

### Task 1: Proto file parser

**Files:**
- Create: `tools/proto_parser.py`
- Test: `tools/test_proto_parser.py`

**Step 1: Write failing test**

```python
# tools/test_proto_parser.py
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from proto_parser import parse_proto_file

def test_parse_message():
    """Parse AudioConfigData.proto — simple 3-field message."""
    result = parse_proto_file("libs/open-androidauto/proto/AudioConfigData.proto")
    assert len(result["messages"]) == 1
    msg = result["messages"]["AudioConfig"]
    assert len(msg["fields"]) == 3
    assert msg["fields"][0] == {
        "number": 1, "name": "sample_rate", "type": "uint32",
        "cardinality": "required", "message_type": None
    }

def test_parse_enum():
    """Parse StatusEnum.proto — enum wrapped in message."""
    result = parse_proto_file("libs/open-androidauto/proto/StatusEnum.proto")
    assert "Status" in result["messages"]
    msg = result["messages"]["Status"]
    assert "Enum" in msg["enums"]
    assert msg["enums"]["Enum"]["OK"] == 0

def test_parse_repeated():
    """Parse InputChannelData.proto — repeated fields."""
    result = parse_proto_file("libs/open-androidauto/proto/InputChannelData.proto")
    msg = result["messages"]["InputChannel"]
    keycodes = msg["fields"][0]
    assert keycodes["cardinality"] == "repeated"
    touch = msg["fields"][1]
    assert touch["cardinality"] == "repeated"
    assert touch["message_type"] == "TouchConfig"

def test_parse_packed():
    """Parse BluetoothChannelData.proto — packed repeated field."""
    result = parse_proto_file("libs/open-androidauto/proto/BluetoothChannelData.proto")
    msg = result["messages"]["BluetoothChannel"]
    pairing = msg["fields"][1]
    assert pairing["cardinality"] == "packed"

if __name__ == "__main__":
    test_parse_message()
    test_parse_enum()
    test_parse_repeated()
    test_parse_packed()
    print("All proto_parser tests passed")
```

**Step 2: Run test to verify it fails**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy && python3 tools/test_proto_parser.py`
Expected: FAIL — `ModuleNotFoundError: No module named 'proto_parser'`

**Step 3: Write implementation**

```python
# tools/proto_parser.py
"""Parse proto2 .proto files into structured field data."""
import re
from pathlib import Path

def parse_proto_file(filepath):
    """Parse a .proto file, returning messages and their fields."""
    content = Path(filepath).read_text()
    result = {"package": "", "messages": {}, "imports": []}

    # Package
    pkg = re.search(r'package\s+([\w.]+);', content)
    if pkg:
        result["package"] = pkg.group(1)

    # Imports
    for m in re.finditer(r'import\s+"([^"]+)"', content):
        result["imports"].append(m.group(1))

    # Messages (handles one level of nesting for enum-in-message)
    for msg_match in re.finditer(
        r'message\s+(\w+)\s*\{([^}]*(?:\{[^}]*\}[^}]*)*)\}', content
    ):
        msg_name = msg_match.group(1)
        msg_body = msg_match.group(2)

        fields = []
        enums = {}

        # Parse fields
        for f in re.finditer(
            r'(required|optional|repeated)\s+'
            r'([\w.]+)\s+(\w+)\s*=\s*(\d+)\s*'
            r'(?:\[packed\s*=\s*true\])?',
            msg_body
        ):
            cardinality = f.group(1)
            field_type = f.group(2)
            field_name = f.group(3)
            field_num = int(f.group(4))

            # Check for [packed=true]
            field_text = msg_body[f.start():f.end() + 50]
            if "[packed=true]" in field_text or "[packed = true]" in field_text:
                cardinality = "packed"

            # Determine if type is a message/enum reference
            msg_type = None
            base_types = {
                "double", "float", "int32", "int64", "uint32", "uint64",
                "sint32", "sint64", "fixed32", "fixed64", "sfixed32",
                "sfixed64", "bool", "string", "bytes"
            }
            # Strip enum wrapper (e.g., "enums.Status.Enum" -> just flag as enum ref)
            clean_type = field_type.split(".")[-1] if "." in field_type else field_type
            if clean_type not in base_types:
                msg_type = field_type

            fields.append({
                "number": field_num,
                "name": field_name,
                "type": clean_type if clean_type in base_types else field_type,
                "cardinality": cardinality,
                "message_type": msg_type,
            })

        # Parse enums inside this message
        for e in re.finditer(r'enum\s+(\w+)\s*\{([^}]+)\}', msg_body):
            enum_name = e.group(1)
            enum_body = e.group(2)
            values = {}
            for v in re.finditer(r'(\w+)\s*=\s*(-?\d+)', enum_body):
                values[v.group(1)] = int(v.group(2))
            enums[enum_name] = values

        result["messages"][msg_name] = {"fields": fields, "enums": enums}

    return result


def parse_all_protos(proto_dir):
    """Parse all .proto files in a directory."""
    results = {}
    for proto_file in sorted(Path(proto_dir).glob("*.proto")):
        parsed = parse_proto_file(str(proto_file))
        for msg_name in parsed["messages"]:
            results[msg_name] = {
                "file": proto_file.name,
                **parsed["messages"][msg_name],
                "package": parsed["package"],
            }
    return results
```

**Step 4: Run tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy && python3 tools/test_proto_parser.py`
Expected: All tests pass

**Step 5: Commit**

```bash
git add tools/proto_parser.py tools/test_proto_parser.py
git commit -m "feat: add proto2 file parser for validation pipeline"
```

---

### Task 2: Manual match overrides + match loader

**Files:**
- Create: `tools/proto_matches.json` (manual overrides)
- Create: `tools/match_loader.py`
- Test: `tools/test_match_loader.py`

**Step 1: Write failing test**

```python
# tools/test_match_loader.py
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from match_loader import load_matches

def test_load_matches():
    """Load and merge auto-matches with manual overrides."""
    matches = load_matches()
    # Should have ChannelDescriptor -> wbw (from auto or manual)
    assert "ChannelDescriptorData" in matches or "ChannelDescriptor" in matches
    # Check structure
    for proto_name, info in list(matches.items())[:1]:
        assert "apk_class" in info
        assert "score" in info
        assert "source" in info  # "auto" or "manual"

def test_manual_override():
    """Manual overrides should take precedence."""
    matches = load_matches()
    # wbw is manually confirmed
    wbw_match = None
    for name, info in matches.items():
        if info["apk_class"] == "wbw":
            wbw_match = info
            break
    assert wbw_match is not None
    assert wbw_match["source"] == "manual"

if __name__ == "__main__":
    test_load_matches()
    test_manual_override()
    print("All match_loader tests passed")
```

**Step 2: Run test to verify it fails**

Run: `python3 tools/test_match_loader.py`
Expected: FAIL

**Step 3: Create manual overrides file**

```json
// tools/proto_matches.json — manually verified APK class mappings
// These override auto-matches from apk_protos.json
{
    "ChannelDescriptor": {"apk_class": "wbw", "score": 1.0, "note": "Verified via wire dump 2026-02-25"},
    "AVChannel": {"apk_class": "vys", "score": 1.0, "note": "Verified via wire dump"},
    "InputChannel": {"apk_class": "vya", "score": 1.0, "note": "Verified via wire dump"},
    "SensorChannel": {"apk_class": "wbu", "score": 1.0, "note": "Verified via wire dump"},
    "BluetoothChannel": {"apk_class": "vwc", "score": 1.0, "note": "Verified via wire dump"},
    "NavigationChannel": {"apk_class": "vzr", "score": 1.0, "note": "Verified via wire dump"},
    "WifiChannel": {"apk_class": "wdh", "score": 1.0, "note": "Verified via wire dump"},
    "AVInputChannel": {"apk_class": "vyt", "score": 1.0, "note": "Verified via wire dump"},
    "AudioConfig": {"apk_class": "vvp", "score": 1.0, "note": "Field-perfect match"},
    "VideoConfig": {"apk_class": "wcz", "score": 1.0, "note": "11 fields match"},
    "TouchConfig": {"apk_class": "vxn", "score": 0.8, "note": "2/3 fields match (missing bool)"},
    "ServiceDiscoveryResponse": {"apk_class": "wby", "score": 1.0, "note": "Top-level response"},
    "ServiceDiscoveryRequest": {"apk_class": "aahi", "score": 1.0, "note": "Top-level request"},
    "PhoneStatusChannel": {"apk_class": "vyr", "score": 1.0, "note": "Empty message, confirmed"},
    "NotificationChannel": {"apk_class": "wah", "score": 1.0, "note": "Empty message, confirmed"}
}
```

**Step 4: Write match_loader.py**

```python
# tools/match_loader.py
"""Load APK class matches from auto-detection + manual overrides."""
import json
from pathlib import Path

TOOLS_DIR = Path(__file__).parent
APK_PROTOS_JSON = TOOLS_DIR / "proto_decode_output" / "apk_protos.json"
MANUAL_OVERRIDES = TOOLS_DIR / "proto_matches.json"


def load_matches():
    """Load matches, with manual overrides taking precedence."""
    matches = {}

    # Load auto-matches from apk_protos.json
    if APK_PROTOS_JSON.exists():
        with open(APK_PROTOS_JSON) as f:
            data = json.load(f)
        for m in data.get("matches", []):
            # Use our_message (e.g., "ChannelDescriptor") as key
            key = m.get("our_message", m.get("our_proto", ""))
            matches[key] = {
                "apk_class": m["apk_class"],
                "score": m["score"],
                "source": "auto",
                "our_proto_file": m.get("our_proto", ""),
                "diffs": m.get("diffs", []),
            }

    # Load manual overrides (take precedence)
    if MANUAL_OVERRIDES.exists():
        with open(MANUAL_OVERRIDES) as f:
            overrides = json.load(f)
        for msg_name, info in overrides.items():
            matches[msg_name] = {
                "apk_class": info["apk_class"],
                "score": info.get("score", 1.0),
                "source": "manual",
                "note": info.get("note", ""),
                "diffs": [],
            }

    return matches
```

**Step 5: Run tests, commit**

Run: `python3 tools/test_match_loader.py`

```bash
git add tools/proto_matches.json tools/match_loader.py tools/test_match_loader.py
git commit -m "feat: add match loader with manual override support"
```

---

### Task 3: Field-level comparator

**Files:**
- Create: `tools/proto_comparator.py`
- Test: `tools/test_proto_comparator.py`

**Step 1: Write failing test**

```python
# tools/test_proto_comparator.py
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from proto_comparator import compare_fields

def test_perfect_match():
    """AudioConfig: our 3 uint32 fields vs APK vvp 3 uint32 fields."""
    our_fields = [
        {"number": 1, "name": "sample_rate", "type": "uint32", "cardinality": "required", "message_type": None},
        {"number": 2, "name": "bit_depth", "type": "uint32", "cardinality": "required", "message_type": None},
        {"number": 3, "name": "channel_count", "type": "uint32", "cardinality": "required", "message_type": None},
    ]
    apk_fields = [
        {"wire_field": 1, "type": "uint32", "cardinality": "singular", "flags": {"bit8_required": True}},
        {"wire_field": 2, "type": "uint32", "cardinality": "singular", "flags": {"bit8_required": True}},
        {"wire_field": 3, "type": "uint32", "cardinality": "singular", "flags": {"bit8_required": True}},
    ]
    result = compare_fields(our_fields, apk_fields)
    assert result["status"] == "verified"
    assert len(result["mismatches"]) == 0

def test_type_mismatch():
    """Detect int32 vs uint32 difference."""
    our_fields = [
        {"number": 1, "name": "value", "type": "int32", "cardinality": "required", "message_type": None},
    ]
    apk_fields = [
        {"wire_field": 1, "type": "uint32", "cardinality": "singular", "flags": {"bit8_required": True}},
    ]
    result = compare_fields(our_fields, apk_fields)
    assert result["status"] == "partial"
    assert any("type" in m["issue"] for m in result["mismatches"])

def test_extra_apk_field():
    """APK has field 3 but we don't — should be partial."""
    our_fields = [
        {"number": 1, "name": "width", "type": "uint32", "cardinality": "required", "message_type": None},
        {"number": 2, "name": "height", "type": "uint32", "cardinality": "required", "message_type": None},
    ]
    apk_fields = [
        {"wire_field": 1, "type": "int32", "cardinality": "singular", "flags": {"bit8_required": True}},
        {"wire_field": 2, "type": "int32", "cardinality": "singular", "flags": {"bit8_required": True}},
        {"wire_field": 3, "type": "bool", "cardinality": "singular", "flags": {"bit8_required": False}},
    ]
    result = compare_fields(our_fields, apk_fields)
    assert result["status"] == "partial"
    assert any("missing" in m["issue"] for m in result["mismatches"])

def test_cardinality_mismatch():
    """Detect optional vs repeated difference."""
    our_fields = [
        {"number": 1, "name": "items", "type": "uint32", "cardinality": "optional", "message_type": None},
    ]
    apk_fields = [
        {"wire_field": 1, "type": "uint32", "cardinality": "repeated", "flags": {"bit8_required": False}},
    ]
    result = compare_fields(our_fields, apk_fields)
    assert result["status"] == "partial"
    assert any("cardinality" in m["issue"] for m in result["mismatches"])

if __name__ == "__main__":
    test_perfect_match()
    test_type_mismatch()
    test_extra_apk_field()
    test_cardinality_mismatch()
    print("All proto_comparator tests passed")
```

**Step 2: Run test to verify it fails**

Run: `python3 tools/test_proto_comparator.py`
Expected: FAIL

**Step 3: Write implementation**

```python
# tools/proto_comparator.py
"""Compare our proto field definitions against APK decoder output."""

# Types that are compatible on the wire (same varint encoding)
COMPATIBLE_TYPES = {
    frozenset({"int32", "uint32", "sint32", "enum", "bool"}),
    frozenset({"int64", "uint64", "sint64"}),
    frozenset({"fixed32", "sfixed32", "float"}),
    frozenset({"fixed64", "sfixed64", "double"}),
    frozenset({"string", "bytes", "message"}),
}

# Cardinality mappings: our proto2 keywords -> APK decoder output
CARDINALITY_MAP = {
    "required": "singular",
    "optional": "singular",
    "repeated": "repeated",
    "packed": "packed",
}


def types_compatible(our_type, apk_type):
    """Check if two protobuf types are wire-compatible."""
    if our_type == apk_type:
        return True
    # Normalize: strip enum wrapper syntax
    our_clean = our_type.split(".")[-1] if "." in our_type else our_type
    apk_clean = apk_type.split(".")[-1] if "." in apk_type else apk_type
    if our_clean == apk_clean:
        return True
    # Check compatibility groups
    for group in COMPATIBLE_TYPES:
        if our_clean in group and apk_clean in group:
            return True
    return False


def compare_fields(our_fields, apk_fields):
    """Compare our proto fields against APK decoder fields.

    Returns dict with:
        status: "verified" | "partial"
        mismatches: list of {field, issue, our_value, apk_value}
        matched_count: number of fields that match
        our_only: fields in our proto but not in APK
        apk_only: fields in APK but not in our proto
    """
    mismatches = []
    our_by_num = {f["number"]: f for f in our_fields}
    apk_by_num = {f["wire_field"]: f for f in apk_fields}

    our_nums = set(our_by_num.keys())
    apk_nums = set(apk_by_num.keys())

    matched_count = 0

    # Fields in both
    for num in our_nums & apk_nums:
        ours = our_by_num[num]
        apk = apk_by_num[num]
        field_ok = True

        # Type check
        our_type = ours["type"]
        apk_type = apk["type"]
        # If our field has a message_type, it's a message reference
        if ours["message_type"]:
            our_type = "message"

        if not types_compatible(our_type, apk_type):
            mismatches.append({
                "field": num,
                "issue": "type mismatch",
                "our_value": ours["type"],
                "apk_value": apk["type"],
            })
            field_ok = False

        # Cardinality check
        our_card = CARDINALITY_MAP.get(ours["cardinality"], ours["cardinality"])
        apk_card = apk["cardinality"]
        if our_card != apk_card:
            mismatches.append({
                "field": num,
                "issue": "cardinality mismatch",
                "our_value": ours["cardinality"],
                "apk_value": apk_card,
            })
            field_ok = False

        if field_ok:
            matched_count += 1

    # Fields only in our proto
    for num in our_nums - apk_nums:
        mismatches.append({
            "field": num,
            "issue": "extra field in our proto (not in APK)",
            "our_value": our_by_num[num]["name"],
            "apk_value": None,
        })

    # Fields only in APK
    for num in apk_nums - our_nums:
        mismatches.append({
            "field": num,
            "issue": "missing field (exists in APK)",
            "our_value": None,
            "apk_value": apk_by_num[num]["type"],
        })

    status = "verified" if len(mismatches) == 0 else "partial"

    return {
        "status": status,
        "mismatches": sorted(mismatches, key=lambda m: m["field"]),
        "matched_count": matched_count,
        "our_field_count": len(our_fields),
        "apk_field_count": len(apk_fields),
        "our_only": sorted(our_nums - apk_nums),
        "apk_only": sorted(apk_nums - our_nums),
    }
```

**Step 4: Run tests, commit**

Run: `python3 tools/test_proto_comparator.py`

```bash
git add tools/proto_comparator.py tools/test_proto_comparator.py
git commit -m "feat: add field-level proto comparator"
```

---

### Task 4: Main validation script + report generator

**Files:**
- Create: `tools/validate_protos.py`
- Test: manual run

**Step 1: Write the main script**

```python
#!/usr/bin/env python3
"""
Validate our proto definitions against the decompiled AA APK v16.1.

Usage:
    python3 tools/validate_protos.py                    # full validation
    python3 tools/validate_protos.py --message AudioConfig  # single message
    python3 tools/validate_protos.py --summary          # summary only
"""

import json
import sys
import os
from pathlib import Path
from datetime import date

# Add tools dir to path
TOOLS_DIR = Path(__file__).parent
sys.path.insert(0, str(TOOLS_DIR))

from proto_parser import parse_all_protos
from match_loader import load_matches
from proto_comparator import compare_fields
from descriptor_decoder import decode_class

PROTO_DIR = TOOLS_DIR.parent / "libs" / "open-androidauto" / "proto"
REPORT_JSON = TOOLS_DIR / "proto_validation_report.json"
REPORT_MD = TOOLS_DIR.parent / "docs" / "proto-validation-report.md"


def validate_all(filter_message=None):
    """Run full validation pipeline."""
    # Step 1: Parse our protos
    our_protos = parse_all_protos(str(PROTO_DIR))
    print(f"Parsed {len(our_protos)} messages from {PROTO_DIR}")

    # Step 2: Load matches
    matches = load_matches()
    print(f"Loaded {len(matches)} APK class matches")

    # Step 3: Validate each matched proto
    results = []
    for msg_name, msg_data in sorted(our_protos.items()):
        if filter_message and msg_name != filter_message:
            continue

        match = matches.get(msg_name)
        if not match:
            results.append({
                "message": msg_name,
                "file": msg_data["file"],
                "status": "unmatched",
                "apk_class": None,
                "match_source": None,
                "match_score": 0,
                "comparison": None,
            })
            continue

        # Decode APK class
        apk_class = match["apk_class"]
        decoded = decode_class(apk_class)
        if not decoded:
            results.append({
                "message": msg_name,
                "file": msg_data["file"],
                "status": "error",
                "apk_class": apk_class,
                "match_source": match["source"],
                "match_score": match["score"],
                "comparison": None,
                "error": f"Failed to decode APK class {apk_class}",
            })
            continue

        # Compare fields
        comparison = compare_fields(msg_data["fields"], decoded["fields"])

        results.append({
            "message": msg_name,
            "file": msg_data["file"],
            "status": comparison["status"],
            "apk_class": apk_class,
            "match_source": match["source"],
            "match_score": match["score"],
            "comparison": comparison,
        })

    return results


def generate_json_report(results):
    """Write machine-readable JSON report."""
    summary = {
        "date": str(date.today()),
        "total": len(results),
        "verified": sum(1 for r in results if r["status"] == "verified"),
        "partial": sum(1 for r in results if r["status"] == "partial"),
        "unmatched": sum(1 for r in results if r["status"] == "unmatched"),
        "error": sum(1 for r in results if r["status"] == "error"),
    }
    report = {"summary": summary, "results": results}
    with open(REPORT_JSON, "w") as f:
        json.dump(report, f, indent=2)
    print(f"JSON report: {REPORT_JSON}")


def generate_md_report(results):
    """Write human-readable Markdown report."""
    verified = [r for r in results if r["status"] == "verified"]
    partial = [r for r in results if r["status"] == "partial"]
    unmatched = [r for r in results if r["status"] == "unmatched"]
    errors = [r for r in results if r["status"] == "error"]

    lines = [
        f"# Proto Validation Report",
        f"",
        f"Generated: {date.today()}",
        f"",
        f"## Summary",
        f"",
        f"| Status | Count |",
        f"|--------|-------|",
        f"| Verified | {len(verified)} |",
        f"| Partial (diffs) | {len(partial)} |",
        f"| Unmatched | {len(unmatched)} |",
        f"| Error | {len(errors)} |",
        f"| **Total** | **{len(results)}** |",
        f"",
    ]

    if verified:
        lines.append("## Verified (all fields match)")
        lines.append("")
        for r in verified:
            src = f" [{r['match_source']}]" if r["match_source"] else ""
            lines.append(f"- **{r['message']}** ({r['file']}) -> `{r['apk_class']}`{src}")
        lines.append("")

    if partial:
        lines.append("## Partial (field diffs)")
        lines.append("")
        for r in partial:
            comp = r["comparison"]
            src = f" [{r['match_source']}]" if r["match_source"] else ""
            lines.append(f"### {r['message']} ({r['file']}) -> `{r['apk_class']}`{src}")
            lines.append(f"")
            lines.append(f"Matched: {comp['matched_count']}/{comp['our_field_count']} our fields, "
                         f"APK has {comp['apk_field_count']} fields")
            lines.append("")
            if comp["mismatches"]:
                lines.append("| Field | Issue | Ours | APK |")
                lines.append("|-------|-------|------|-----|")
                for m in comp["mismatches"]:
                    lines.append(f"| {m['field']} | {m['issue']} | {m['our_value']} | {m['apk_value']} |")
                lines.append("")

    if unmatched:
        lines.append("## Unmatched (no APK class found)")
        lines.append("")
        for r in unmatched:
            lines.append(f"- {r['message']} ({r['file']})")
        lines.append("")

    if errors:
        lines.append("## Errors")
        lines.append("")
        for r in errors:
            lines.append(f"- {r['message']} -> `{r['apk_class']}`: {r.get('error', 'unknown')}")
        lines.append("")

    with open(REPORT_MD, "w") as f:
        f.write("\n".join(lines))
    print(f"Markdown report: {REPORT_MD}")


def print_summary(results):
    """Print summary to stdout."""
    verified = sum(1 for r in results if r["status"] == "verified")
    partial = sum(1 for r in results if r["status"] == "partial")
    unmatched = sum(1 for r in results if r["status"] == "unmatched")
    errors = sum(1 for r in results if r["status"] == "error")

    print(f"\n{'='*50}")
    print(f"Proto Validation Results")
    print(f"{'='*50}")
    print(f"  Verified:  {verified}")
    print(f"  Partial:   {partial}")
    print(f"  Unmatched: {unmatched}")
    print(f"  Errors:    {errors}")
    print(f"  Total:     {len(results)}")
    print(f"{'='*50}")

    if partial:
        print(f"\nPartial matches (need review):")
        for r in partial:
            comp = r["comparison"]
            issues = ", ".join(f"f{m['field']}:{m['issue']}" for m in comp["mismatches"][:3])
            more = f" +{len(comp['mismatches'])-3} more" if len(comp["mismatches"]) > 3 else ""
            print(f"  {r['message']} -> {r['apk_class']}: {issues}{more}")


def main():
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
```

**Step 2: Run it**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy && python3 tools/validate_protos.py --summary`
Expected: Summary output showing verified/partial/unmatched/error counts

**Step 3: Run full report**

Run: `python3 tools/validate_protos.py`
Expected: Creates `tools/proto_validation_report.json` and `docs/proto-validation-report.md`

**Step 4: Commit**

```bash
git add tools/validate_protos.py
git commit -m "feat: add proto validation pipeline with report generation"
```

---

### Task 5: Run validation, review results, commit report

**Step 1: Run the full validation**

Run: `python3 tools/validate_protos.py`

**Step 2: Review the Markdown report**

Read `docs/proto-validation-report.md` and look for:
- How many verified vs partial?
- What are the most common mismatches?
- Are any critical protos (ServiceDiscovery, ChannelDescriptor, AV, Input) in "partial" or "error"?

**Step 3: Triage partials**

For each partial match, decide:
- Is this a real proto bug we should fix?
- Is it an APK field we don't use yet (safe to ignore)?
- Is it a wrong APK match (need to update proto_matches.json)?

**Step 4: Commit the report**

```bash
git add tools/proto_validation_report.json docs/proto-validation-report.md
git commit -m "docs: initial proto validation report — N verified, M partial, K unmatched"
```

---

### Task 6: Fix issues found by validation

This is an iterative task based on what Task 5 reveals. For each fixable issue:

**Step 1:** Update the .proto file to match APK
**Step 2:** Update any C++ consumers if accessors changed
**Step 3:** Build: `cmake --build build -j$(nproc)`
**Step 4:** Test: `ctest --output-on-failure`
**Step 5:** Rerun validation: `python3 tools/validate_protos.py`
**Step 6:** Commit the fix

---
