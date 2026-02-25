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


def test_compatible_int_types():
    """int32 vs uint32 are wire-compatible — should verify, not mismatch."""
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
    assert result["status"] == "verified"
    assert len(result["mismatches"]) == 0


def test_compatible_varint_enum_types():
    """sint32 and enum share varint wire type."""
    our_fields = [
        {
            "number": 1,
            "name": "mode",
            "type": "sint32",
            "cardinality": "optional",
            "message_type": None,
        },
    ]
    apk_fields = [
        {"wire_field": 1, "type": "enum", "cardinality": "singular", "flags": {"bit8_required": False}},
    ]
    result = compare_fields(our_fields, apk_fields)
    assert result["status"] == "verified"
    assert len(result["mismatches"]) == 0


def test_compatible_bytes_message_types():
    """bytes and message are both length-delimited and should be wire-compatible."""
    our_fields = [
        {
            "number": 2,
            "name": "payload",
            "type": "bytes",
            "cardinality": "optional",
            "message_type": None,
        },
    ]
    apk_fields = [
        {"wire_field": 2, "type": "message", "cardinality": "singular", "flags": {"bit8_required": False}},
    ]
    result = compare_fields(our_fields, apk_fields)
    assert result["status"] == "verified"
    assert len(result["mismatches"]) == 0


def test_incompatible_type_mismatch():
    """string vs int32 should be a real type mismatch."""
    our_fields = [
        {
            "number": 1,
            "name": "value",
            "type": "string",
            "cardinality": "required",
            "message_type": None,
        },
    ]
    apk_fields = [
        {"wire_field": 1, "type": "int32", "cardinality": "singular", "flags": {"bit8_required": True}},
    ]
    result = compare_fields(our_fields, apk_fields)
    assert result["status"] == "partial"
    assert any("type" in m["issue"] for m in result["mismatches"])


def test_enum_type_compatibility():
    """Our named enum type (Enum) should match APK's 'enum' wire type."""
    our_fields = [
        {
            "number": 1,
            "name": "focus_type",
            "type": "Enum",
            "cardinality": "required",
            "message_type": "Enum",
        },
    ]
    apk_fields = [
        {"wire_field": 1, "type": "enum", "cardinality": "singular", "flags": {"bit8_required": True}},
    ]
    result = compare_fields(our_fields, apk_fields)
    assert result["status"] == "verified"
    assert len(result["mismatches"]) == 0


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


def test_oneof_and_singular_cardinality_are_compatible():
    our_fields = [
        {
            "number": 3,
            "name": "choice",
            "type": "bytes",
            "cardinality": "oneof",
            "message_type": None,
        },
    ]
    apk_fields = [
        {"wire_field": 3, "type": "message", "cardinality": "singular", "flags": {"bit8_required": False}},
    ]
    result = compare_fields(our_fields, apk_fields)
    assert result["status"] == "verified"
    assert len(result["mismatches"]) == 0


def test_malformed_apk_field_entry():
    """Malformed decoded rows should not crash comparison."""
    our_fields = [
        {
            "number": 1,
            "name": "request_id",
            "type": "uint32",
            "cardinality": "optional",
            "message_type": None,
        }
    ]
    apk_fields = [
        {"error": "Ran out of descriptor data at pos 22"},
    ]
    result = compare_fields(our_fields, apk_fields)
    assert result["status"] == "partial"
    assert any("malformed" in m["issue"] for m in result["mismatches"])


if __name__ == "__main__":
    test_perfect_match()
    test_type_mismatch()
    test_extra_apk_field()
    test_cardinality_mismatch()
    test_malformed_apk_field_entry()
    print("All proto_comparator tests passed")
