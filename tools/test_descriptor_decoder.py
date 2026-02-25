import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from descriptor_decoder import decode_descriptor


def test_version_zero_descriptor_without_has_bits_payload():
    """Version-0 descriptors with has_bits_count=0 should not consume per-field has_bits varints."""
    data = {
        "class_name": "zyd",
        "codepoints": [
            0x00,  # version
            0x02,  # field_count
            0x00,  # has_bits_count
            0x00,  # oneof_count
            0x01,  # check_init
            0x02,  # map_fields
            0x02,  # field_count2
            0x00,  # i36
            0x00,  # charAt20
            0x00,  # charAt21
            0x01,  # field 1 wire number
            0x02,  # field 1 type_flags (int64, singular)
            0x02,  # field 2 wire number
            0x04,  # field 2 type_flags (int32, singular)
        ],
        "object_array": [],
        "java_fields": {"b": "long", "c": "int"},
    }

    decoded = decode_descriptor(data)
    assert len(decoded["fields"]) == 2
    assert "error" not in decoded["fields"][0]
    assert "error" not in decoded["fields"][1]
    assert decoded["fields"][0]["wire_field"] == 1
    assert decoded["fields"][0]["type"] == "int64"
    assert decoded["fields"][1]["wire_field"] == 2
    assert decoded["fields"][1]["type"] == "int32"
    assert "has_bits" not in decoded["fields"][0]
    assert "has_bits" not in decoded["fields"][1]

