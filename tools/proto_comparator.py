"""Compare our proto field definitions against APK decoder output."""

from __future__ import annotations

CARDINALITY_MAP = {
    "required": "singular",
    "optional": "singular",
    "repeated": "repeated",
    "packed": "packed",
}

# Wire-compatible integer pairs (same varint encoding for positive values)
COMPATIBLE_INT_PAIRS = {
    frozenset({"uint32", "int32"}),
    frozenset({"uint64", "int64"}),
}

VARINT_COMPATIBLE_TYPES = {"int32", "uint32", "sint32", "enum"}
LENGTH_DELIMITED_COMPATIBLE_TYPES = {"bytes", "message"}

# Cardinality pairs that are wire-compatible (repeated and packed carry same data)
COMPATIBLE_CARDINALITY = {
    frozenset({"repeated", "packed"}),
    frozenset({"singular", "packed"}),  # singular enum in APK can be packed
    frozenset({"oneof", "singular"}),
    frozenset({"repeated", "map"}),     # proto map is wire-encoded as repeated message
}


def _resolve_our_type(field: dict) -> str:
    """Resolve our proto field to a comparable type string.

    Named enum references (message_type set, type ends in 'Enum' or is a known
    enum pattern) become 'enum'. Other message_type references become 'message'.
    Base types pass through unchanged.
    """
    if not field.get("message_type"):
        return field["type"]
    # Our parser stores enum references with type="Enum" (the last segment of
    # e.g. enums.AudioFocusType.Enum). Actual message references have type
    # equal to the message name (e.g. "SensorChannel", "TouchConfig").
    type_name = field["type"]
    if type_name == "Enum" or type_name.endswith("Enum"):
        return "enum"
    return "message"


def types_compatible(our_type: str, apk_type: str) -> bool:
    """Check whether our type and APK type match semantically."""
    if our_type == apk_type:
        return True
    # Wire-compatible integer pairs
    if frozenset({our_type, apk_type}) in COMPATIBLE_INT_PAIRS:
        return True
    # int32/uint32/sint32/enum all encode as varint (wire type 0)
    if our_type in VARINT_COMPATIBLE_TYPES and apk_type in VARINT_COMPATIBLE_TYPES:
        return True
    # bytes/message both encode as length-delimited (wire type 2)
    if our_type in LENGTH_DELIMITED_COMPATIBLE_TYPES and apk_type in LENGTH_DELIMITED_COMPATIBLE_TYPES:
        return True
    # MAP type reports as unknown(-1) — compatible with message (map entries are messages)
    if apk_type == "unknown(-1)" and our_type == "message":
        return True
    return False


def cardinality_compatible(our_card: str, apk_card: str) -> bool:
    """Check whether cardinalities are compatible."""
    if our_card == apk_card:
        return True
    if frozenset({our_card, apk_card}) in COMPATIBLE_CARDINALITY:
        return True
    return False


def compare_fields(our_fields: list[dict], apk_fields: list[dict]) -> dict:
    """Compare our proto fields against APK decoder fields."""
    mismatches = []
    our_by_num = {field["number"]: field for field in our_fields}
    apk_by_num = {}
    malformed_apk_entries = []
    for field in apk_fields:
        wire_field = field.get("wire_field")
        if wire_field is None:
            malformed_apk_entries.append(field)
            continue
        apk_by_num[wire_field] = field

    our_nums = set(our_by_num)
    apk_nums = set(apk_by_num)
    matched_count = 0

    for num in sorted(our_nums & apk_nums):
        ours = our_by_num[num]
        apk = apk_by_num[num]
        field_ok = True

        our_type = _resolve_our_type(ours)
        if not types_compatible(our_type, apk["type"]):
            mismatches.append(
                {
                    "field": num,
                    "issue": "type mismatch",
                    "our_value": f"{ours['type']} (→{our_type})" if our_type != ours["type"] else ours["type"],
                    "apk_value": apk["type"],
                }
            )
            field_ok = False

        our_card = CARDINALITY_MAP.get(ours["cardinality"], ours["cardinality"])
        if not cardinality_compatible(our_card, apk["cardinality"]):
            mismatches.append(
                {
                    "field": num,
                    "issue": "cardinality mismatch",
                    "our_value": ours["cardinality"],
                    "apk_value": apk["cardinality"],
                }
            )
            field_ok = False

        if field_ok:
            matched_count += 1

    for num in sorted(our_nums - apk_nums):
        mismatches.append(
            {
                "field": num,
                "issue": "extra field in our proto (not in APK)",
                "our_value": our_by_num[num]["name"],
                "apk_value": None,
            }
        )

    for num in sorted(apk_nums - our_nums):
        mismatches.append(
            {
                "field": num,
                "issue": "missing field (exists in APK)",
                "our_value": None,
                "apk_value": apk_by_num[num]["type"],
            }
        )

    for bad_entry in malformed_apk_entries:
        mismatches.append(
            {
                "field": -1,
                "issue": "malformed APK field entry",
                "our_value": None,
                "apk_value": bad_entry.get("error") or str(bad_entry),
            }
        )

    return {
        "status": "verified" if not mismatches else "partial",
        "mismatches": sorted(mismatches, key=lambda item: item["field"]),
        "matched_count": matched_count,
        "our_field_count": len(our_fields),
        "apk_field_count": len(apk_fields),
        "our_only": sorted(our_nums - apk_nums),
        "apk_only": sorted(apk_nums - our_nums),
    }
