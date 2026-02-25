from analysis.tools.apk_indexer.run_indexer import run_indexer


def test_run_indexer_builds_versioned_outputs(tmp_path):
    source = tmp_path / "decompiled"
    source.mkdir()
    (source / "apktool.yml").write_text("versionName: 16.1\nversionCode: '1248424'\n")
    (source / "Main.java").write_text(
        'String u = "4de17a00-52cb-11e6-bdf4-0800200c9a66";\n'
    )

    base = run_indexer(source, tmp_path / "analysis")

    assert (base / "apk-source").exists()
    assert (base / "apk-index" / "sqlite" / "apk_index.db").exists()
    assert (base / "apk-index" / "json" / "uuids.json").exists()
    assert (base / "apk-index" / "reports" / "summary.md").exists()
