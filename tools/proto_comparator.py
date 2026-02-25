"""Compare our proto field definitions against APK decoder output."""

from __future__ import annotations

CARDINALITY_MAP = {
    "required": "singular",
    "optional": "singular",
    "repeated": "repeated",
    "packed": "packed",
}


def _normalize_type(field_type: str) -> str:
    return field_type.split(".")[-1] if "." in field_type else field_type


def types_compatible(our_type: str, apk_type: str) -> bool:
    """Check whether our type and APK type match semantically."""
    our_clean = _normalize_type(our_type)
    apk_clean = _normalize_type(apk_type)
    return our_clean == apk_clean


def compare_fields(our_fields: list[dict], apk_fields: list[dict]) -> dict:
    """Compare our proto fields against APK decoder fields."""
    mismatches = []
    our_by_num = {field["number"]: field for field in our_fields}
    apk_by_num = {field["wire_field"]: field for field in apk_fields}

    our_nums = set(our_by_num)
    apk_nums = set(apk_by_num)
    matched_count = 0

    for num in sorted(our_nums & apk_nums):
        ours = our_by_num[num]
        apk = apk_by_num[num]
        field_ok = True

        our_type = "message" if ours.get("message_type") else ours["type"]
        if not types_compatible(our_type, apk["type"]):
            mismatches.append(
                {
                    "field": num,
                    "issue": "type mismatch",
                    "our_value": ours["type"],
                    "apk_value": apk["type"],
                }
            )
            field_ok = False

        our_card = CARDINALITY_MAP.get(ours["cardinality"], ours["cardinality"])
        if our_card != apk["cardinality"]:
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

    return {
        "status": "verified" if not mismatches else "partial",
        "mismatches": sorted(mismatches, key=lambda item: item["field"]),
        "matched_count": matched_count,
        "our_field_count": len(our_fields),
        "apk_field_count": len(apk_fields),
        "our_only": sorted(our_nums - apk_nums),
        "apk_only": sorted(apk_nums - our_nums),
    }
