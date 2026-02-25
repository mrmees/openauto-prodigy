from analysis.tools.apk_indexer.relocate_source import relocate_source


def test_relocate_source_creates_versioned_apk_source(tmp_path):
    src = tmp_path / "decompiled"
    src.mkdir()
    (src / "a.txt").write_text("x")

    dest = relocate_source(src, tmp_path / "analysis", "16.1", "1248424")

    assert (dest / "apk-source" / "a.txt").exists()
