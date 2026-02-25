from analysis.tools.apk_indexer.extract import extract_signals


def test_extract_uuid(tmp_path):
    sample = tmp_path / "A.java"
    sample.write_text('String u = "4de17a00-52cb-11e6-bdf4-0800200c9a66";\n')

    result = extract_signals(tmp_path)

    assert result["uuids"][0]["value"] == "4de17a00-52cb-11e6-bdf4-0800200c9a66"


def test_extract_constant_hex(tmp_path):
    sample = tmp_path / "B.java"
    sample.write_text("int channel = 0x1A2B;\n")

    result = extract_signals(tmp_path)

    assert result["constants"][0]["value"] == "0x1A2B"


def test_extract_proto_access_setter(tmp_path):
    sample = tmp_path / "C.java"
    sample.write_text("builder.setChannelId(3);\n")

    result = extract_signals(tmp_path)

    assert result["proto_accesses"][0]["accessor"] == "setChannelId"


def test_extract_call_edge_like_invocation(tmp_path):
    sample = tmp_path / "D.java"
    sample.write_text("transport.sendMessage(frame);\n")

    result = extract_signals(tmp_path)

    assert result["call_edges"][0]["target"] == "transport.sendMessage"
