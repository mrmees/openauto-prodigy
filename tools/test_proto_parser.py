import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(__file__))
from proto_parser import parse_proto_file


def test_parse_message():
    """Parse AudioConfigData.proto — simple 3-field message."""
    result = parse_proto_file("libs/open-androidauto/proto/AudioConfigData.proto")
    assert len(result["messages"]) == 1
    msg = result["messages"]["AudioConfig"]
    assert len(msg["fields"]) == 3
    assert msg["fields"][0] == {
        "number": 1,
        "name": "sample_rate",
        "type": "uint32",
        "cardinality": "required",
        "message_type": None,
    }


def test_parse_enum():
    """Parse StatusEnum.proto — enum wrapped in message."""
    result = parse_proto_file("libs/open-androidauto/proto/StatusEnum.proto")
    assert "Status" in result["messages"]
    msg = result["messages"]["Status"]
    assert "Enum" in msg["enums"]
    assert msg["enums"]["Enum"]["OK"] == 0
    assert msg["fields"] == [
        {
            "number": 1,
            "name": "value",
            "type": "int32",
            "cardinality": "optional",
            "message_type": None,
        }
    ]


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


def test_parse_oneof_fields():
    """Parse oneof fields without cardinality keyword."""
    result = parse_proto_file("libs/open-androidauto/proto/ConnectionConfigurationData.proto")
    msg = result["messages"]["ConnectionConfiguration"]
    fields_by_num = {field["number"]: field for field in msg["fields"]}
    assert fields_by_num[2]["name"] == "security_config"
    assert fields_by_num[2]["type"] == "ConnectionSecurityConfig"
    assert fields_by_num[2]["cardinality"] == "oneof"
    assert fields_by_num[2]["message_type"] == "ConnectionSecurityConfig"
    assert fields_by_num[6]["name"] == "reserved_config"
    assert fields_by_num[6]["cardinality"] == "oneof"


def test_parse_top_level_enum_only_file(tmp_path: Path):
    """Top-level enum-only proto should synthesize a single int32 wrapper field."""
    proto = tmp_path / "SampleEnum.proto"
    proto.write_text(
        '\n'.join(
            [
                'syntax="proto2";',
                "",
                "package test.enums;",
                "",
                "enum SampleEnum {",
                "  UNKNOWN = 0;",
                "  READY = 1;",
                "}",
                "",
            ]
        ),
        encoding="utf-8",
    )

    result = parse_proto_file(str(proto))
    assert "SampleEnum" in result["messages"]
    fields = result["messages"]["SampleEnum"]["fields"]
    assert fields == [
        {
            "number": 1,
            "name": "value",
            "type": "int32",
            "cardinality": "optional",
            "message_type": None,
        }
    ]
    assert result["messages"]["SampleEnum"]["enums"]["SampleEnum"]["READY"] == 1


if __name__ == "__main__":
    test_parse_message()
    test_parse_enum()
    test_parse_repeated()
    test_parse_packed()
    test_parse_oneof_fields()
    print("All proto_parser tests passed")
