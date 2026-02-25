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


def test_run_indexer_projection_scope_filters_files(tmp_path):
    source = tmp_path / "decompiled"
    projection = source / "sources" / "com" / "google" / "android" / "projection" / "A.java"
    projection.parent.mkdir(parents=True)
    (source / "apktool.yml").write_text("versionName: 16.1\nversionCode: '1248424'\n")
    projection.write_text('String u = "4de17a00-52cb-11e6-bdf4-0800200c9a66";\n')

    non_projection = source / "sources" / "androidx" / "B.java"
    non_projection.parent.mkdir(parents=True)
    non_projection.write_text('String u = "669a0c20-0008-f4bd-e611-cb52007ae14d";\n')

    base = run_indexer(source, tmp_path / "analysis", scope="projection")

    db = base / "apk-index" / "sqlite" / "apk_index.db"
    import sqlite3

    with sqlite3.connect(db) as conn:
        count = conn.execute("select count(*) from uuids").fetchone()[0]
        values = [r[0] for r in conn.execute("select value from uuids").fetchall()]

    assert count == 1
    assert values == ["4de17a00-52cb-11e6-bdf4-0800200c9a66"]
