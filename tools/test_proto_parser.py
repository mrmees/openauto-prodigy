import os
import sys

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
