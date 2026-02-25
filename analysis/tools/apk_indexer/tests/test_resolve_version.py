from analysis.tools.apk_indexer.resolve_version import resolve_version


def test_resolve_from_apktool_yml(tmp_path):
    p = tmp_path / "apktool.yml"
    p.write_text("versionName: 16.1\nversionCode: '1248424'\n")
    name, code = resolve_version(tmp_path)
    assert name == "16.1"
    assert code == "1248424"
