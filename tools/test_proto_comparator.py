import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from proto_comparator import compare_fields


def test_perfect_match():
    """AudioConfig: our 3 uint32 fields vs APK vvp 3 uint32 fields."""
    our_fields = [
        {
            "number": 1,
            "name": "sample_rate",
            "type": "uint32",
            "cardinality": "required",
            "message_type": None,
        },
        {
            "number": 2,
            "name": "bit_depth",
            "type": "uint32",
            "cardinality": "required",
            "message_type": None,
        },
        {
            "number": 3,
            "name": "channel_count",
            "type": "uint32",
            "cardinality": "required",
            "message_type": None,
        },
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
        {
            "number": 1,
            "name": "value",
            "type": "int32",
            "cardinality": "required",
            "message_type": None,
        },
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
        {
            "number": 1,
            "name": "width",
            "type": "uint32",
            "cardinality": "required",
            "message_type": None,
        },
        {
            "number": 2,
            "name": "height",
            "type": "uint32",
            "cardinality": "required",
            "message_type": None,
        },
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
        {
            "number": 1,
            "name": "items",
            "type": "uint32",
            "cardinality": "optional",
            "message_type": None,
        },
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
