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


def test_extract_proto_write_patterns(tmp_path):
    sample = tmp_path / "E.java"
    sample.write_text(
        "xhqVar.b |= 16;\n"
        "xhqVar.g = i7;\n"
        "if (!o.b.H()) { o.t(); }\n"
        "defpackage.xhq xhqVar6 = (defpackage.xhq) o.q();\n"
    )

    result = extract_signals(tmp_path)

    ops = {(row["target"], row["op"]) for row in result["proto_writes"]}
    assert ("xhqVar.b", "|=") in ops
    assert ("xhqVar.g", "=") in ops


def test_extract_projection_scope_filters_non_projection(tmp_path):
    projection = tmp_path / "sources" / "com" / "google" / "android" / "projection" / "A.java"
    projection.parent.mkdir(parents=True)
    projection.write_text('String u = "4de17a00-52cb-11e6-bdf4-0800200c9a66";\n')

    non_projection = tmp_path / "sources" / "androidx" / "B.java"
    non_projection.parent.mkdir(parents=True)
    non_projection.write_text('String u = "669a0c20-0008-f4bd-e611-cb52007ae14d";\n')

    result = extract_signals(tmp_path, scope="projection")

    assert len(result["uuids"]) == 1
    assert result["uuids"][0]["value"] == "4de17a00-52cb-11e6-bdf4-0800200c9a66"


def test_extract_enum_maps(tmp_path):
    sample = tmp_path / "vyn.java"
    sample.write_text(
        "public enum vyn {\n"
        "  MEDIA_CODEC_AUDIO_PCM(1),\n"
        "  MEDIA_CODEC_AUDIO_AAC_LC(2);\n"
        "  public static vyn b(int i2) {\n"
        "    switch (i2) {\n"
        "      case 1:\n"
        "        return MEDIA_CODEC_AUDIO_PCM;\n"
        "      case 2:\n"
        "        return MEDIA_CODEC_AUDIO_AAC_LC;\n"
        "      default:\n"
        "        return null;\n"
        "    }\n"
        "  }\n"
        "}\n"
    )

    result = extract_signals(tmp_path)

    rows = {(r["enum_class"], r["int_value"], r["enum_name"]) for r in result["enum_maps"]}
    assert ("vyn", 1, "MEDIA_CODEC_AUDIO_PCM") in rows
    assert ("vyn", 2, "MEDIA_CODEC_AUDIO_AAC_LC") in rows


def test_extract_switch_maps(tmp_path):
    sample = tmp_path / "Dispatch.java"
    sample.write_text(
        "switch (messageId) {\n"
        "  case 7:\n"
        "    handler.handleAudio(msg);\n"
        "    break;\n"
        "  case 9:\n"
        "    return codec.select(h);\n"
        "  default:\n"
        "    return null;\n"
        "}\n"
    )

    result = extract_signals(tmp_path)

    rows = {(r["switch_expr"], r["case_value"], r["target"]) for r in result["switch_maps"]}
    assert ("messageId", "7", "handler.handleAudio") in rows
    assert ("messageId", "9", "codec.select") in rows
