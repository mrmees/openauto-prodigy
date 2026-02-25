from analysis.tools.apk_indexer.report import write_summary_report


def test_summary_report_contains_top_sections(tmp_path):
    signals = {
        "uuids": [{"file": "A.java", "line": 1, "value": "4de17a00-52cb-11e6-bdf4-0800200c9a66"}],
        "constants": [{"file": "B.java", "line": 2, "value": "0x1A2B"}],
        "proto_accesses": [{"file": "C.java", "line": 3, "accessor": "setChannelId"}],
        "call_edges": [{"file": "D.java", "line": 4, "target": "transport.sendMessage"}],
        "proto_writes": [{"file": "E.java", "line": 5, "target": "xhqVar.b", "op": "|=", "value": "16"}],
        "enum_maps": [{"file": "vyn.java", "line": 9, "enum_class": "vyn", "int_value": 1, "enum_name": "MEDIA_CODEC_AUDIO_PCM"}],
        "switch_maps": [{"file": "Dispatch.java", "line": 2, "switch_expr": "messageId", "case_value": "7", "target": "handler.handleAudio"}],
    }
    out = tmp_path / "summary.md"
    write_summary_report(out, signals)

    text = out.read_text()
    assert "Top UUIDs" in text
    assert "Top Constants" in text
    assert "Top Proto Accessors" in text
    assert "Top Proto Writes" in text
    assert "Top Enum Maps" in text
    assert "Top Switch Cases" in text
    assert "Top Call Targets" in text
