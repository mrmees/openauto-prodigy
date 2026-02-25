from analysis.tools.apk_indexer.resolve_version import resolve_version


def test_resolve_from_apktool_yml(tmp_path):
    p = tmp_path / "apktool.yml"
    p.write_text("versionName: 16.1\nversionCode: '1248424'\n")
    name, code = resolve_version(tmp_path)
    assert name == "16.1"
    assert code == "1248424"


def test_resolve_from_manifest_json_in_root(tmp_path):
    p = tmp_path / "manifest.json"
    p.write_text('{"version_name":"16.1.660414-release","version_code":"161660414"}')

    name, code = resolve_version(tmp_path)

    assert name == "16.1.660414-release"
    assert code == "161660414"


def test_resolve_from_manifest_json_in_parent(tmp_path):
    decompiled = tmp_path / "decompiled"
    decompiled.mkdir()
    p = tmp_path / "manifest.json"
    p.write_text('{"version_name":"16.1.660414-release","version_code":"161660414"}')

    name, code = resolve_version(decompiled)

    assert name == "16.1.660414-release"
    assert code == "161660414"
